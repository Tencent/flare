// Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of the
// License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include "flare/rpc/internal/stream.h"

#include <deque>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "flare/base/chrono.h"
#include "flare/fiber/latch.h"
#include "flare/fiber/timer.h"
#include "flare/testing/main.h"

using namespace std::literals;

namespace flare {

auto Later(std::chrono::nanoseconds ns) { return ReadSteadyClock() + ns; }

class IntReaderProvider : public StreamReaderProvider<int> {
 public:
  IntReaderProvider() {
    for (int i = 1; i <= 5; ++i) {
      data_.push_back(i);
    }
    data_.push_back(StreamError::EndOfStream);
  }

  void SetExpiration(
      std::chrono::steady_clock::time_point expires_at) override {
    CHECK(!"Unexpected.");
  }

  void Peek(Function<void(Expected<int, StreamError>*)> cb) override {
    fiber::SetDetachedTimer(Later(1ms),
                            [this, cb = std::move(cb)] { cb(&data_.front()); });
  }

  void Read(Function<void(Expected<int, StreamError>)> cb) override {
    fiber::SetDetachedTimer(Later(1ms), [this, cb = std::move(cb)] {
      auto v = data_.front();
      data_.pop_front();
      cb(v);
    });
  }

  void Close(Function<void()> cb) override { cb(); }

 private:
  std::deque<Expected<int, StreamError>> data_;
};

class IntWriterProvider : public StreamWriterProvider<int> {
 public:
  void SetExpiration(
      std::chrono::steady_clock::time_point expires_at) override {
    CHECK(!"Unexpected.");
  }

  void Write(int object, bool last, Function<void(bool)> cb) override {
    fiber::SetDetachedTimer(Later(1ms), [this, cb = std::move(cb), object] {
      written_.push_back(object);
      cb(true);
    });
  }

  void Close(Function<void()> cb) override { cb(); }

  std::vector<int> written_;
};

class ExpiringReaderProvider : public StreamReaderProvider<int> {
 public:
  void SetExpiration(
      std::chrono::steady_clock::time_point expires_at) override {
    fiber::SetDetachedTimer(expires_at, [this] { latch_.count_down(); });
  }

  void Peek(Function<void(Expected<int, StreamError>*)> cb) override {
    latch_.wait();
    Expected<int, StreamError> ec(StreamError::EndOfStream);
    cb(&ec);
  }
  void Read(Function<void(Expected<int, StreamError>)> cb) override {
    latch_.wait();
    cb(StreamError::EndOfStream);
  }
  void Close(Function<void()> cb) override {
    latch_.wait();
    cb();
  }

 private:
  fiber::Latch latch_{1};
};

class ExpiringWriterProvider : public StreamWriterProvider<int> {
 public:
  void SetExpiration(
      std::chrono::steady_clock::time_point expires_at) override {
    fiber::SetDetachedTimer(expires_at, [this] { latch_.count_down(); });
  }

  void Write(int object, bool last, Function<void(bool)> cb) override {
    latch_.wait();
    cb(false);
  }

  void Close(Function<void()> cb) override {
    latch_.wait();
    cb();
  }

 private:
  fiber::Latch latch_{1};
};

class Event {
 public:
  void Wait() {
    while (!ev_) {
    }
    ev_ = false;
  }
  void Notify() { ev_ = true; }

 private:
  std::atomic<bool> ev_{};
};

TEST(AsyncStreamReader, All) {
  AsyncStreamReader<int> sr(MakeRefCounted<IntReaderProvider>());

  ASSERT_EQ(1, **fiber::BlockingGet(sr.Peek()));
  ASSERT_EQ(1, *fiber::BlockingGet(sr.Read()));
  ASSERT_EQ(2, *fiber::BlockingGet(sr.Read()));
  ASSERT_EQ(3, *fiber::BlockingGet(sr.Read()));
  ASSERT_EQ(4, *fiber::BlockingGet(sr.Read()));
  ASSERT_EQ(5, *fiber::BlockingGet(sr.Read()));
  ASSERT_EQ(StreamError::EndOfStream, fiber::BlockingGet(sr.Peek())->error());

  fiber::BlockingGet(sr.Close());
}

TEST(AsyncStreamWriter, All) {
  auto provider = MakeRefCounted<IntWriterProvider>();
  AsyncStreamWriter<int> sw(provider);

  ASSERT_TRUE(fiber::BlockingGet(sw.Write(1)));
  ASSERT_TRUE(fiber::BlockingGet(sw.Write(2)));
  ASSERT_TRUE(fiber::BlockingGet(sw.Write(3)));
  ASSERT_TRUE(fiber::BlockingGet(sw.Write(4)));
  ASSERT_TRUE(fiber::BlockingGet(sw.Write(5)));
  ASSERT_THAT(provider->written_, ::testing::ElementsAre(1, 2, 3, 4, 5));

  fiber::BlockingGet(sw.Close());
}

TEST(AsyncStreamReader, Expiration) {
  AsyncStreamReader<int> sr(MakeRefCounted<ExpiringReaderProvider>());
  sr.SetExpiration(Later(1s));
  auto start = ReadSteadyClock();
  fiber::BlockingGet(sr.Read());
  ASSERT_NEAR((ReadSteadyClock() - start) / 1ms, 1000, 100);
  fiber::BlockingGet(sr.Close());
}

TEST(AsyncStreamWriter, Expiration) {
  AsyncStreamWriter<int> sw(MakeRefCounted<ExpiringWriterProvider>());
  sw.SetExpiration(Later(1s));
  auto start = ReadSteadyClock();
  fiber::BlockingGet(sw.Write(1));
  ASSERT_NEAR((ReadSteadyClock() - start) / 1ms, 1000, 100);
  fiber::BlockingGet(sw.Close());
}

TEST(StreamReader, All) {
  StreamReader<int> sr(MakeRefCounted<IntReaderProvider>());

  ASSERT_EQ(1, **sr.Peek());
  ASSERT_EQ(1, *sr.Read());
  ASSERT_EQ(2, *sr.Read());
  ASSERT_EQ(3, *sr.Read());
  ASSERT_EQ(4, *sr.Read());
  ASSERT_EQ(5, *sr.Read());
  ASSERT_EQ(StreamError::EndOfStream, sr.Peek()->error());

  sr.Close();
}

TEST(StreamWriter, All) {
  auto provider = MakeRefCounted<IntWriterProvider>();
  StreamWriter<int> sw(provider);

  ASSERT_TRUE(sw.Write(1));
  ASSERT_TRUE(sw.Write(2));
  ASSERT_TRUE(sw.Write(3));
  ASSERT_TRUE(sw.Write(4));
  ASSERT_TRUE(sw.Write(5));
  ASSERT_THAT(provider->written_, ::testing::ElementsAre(1, 2, 3, 4, 5));

  sw.Close();
}

TEST(StreamReader, Expiration) {
  StreamReader<int> sr(MakeRefCounted<ExpiringReaderProvider>());
  sr.SetExpiration(Later(1s));
  auto start = ReadSteadyClock();
  sr.Read();
  ASSERT_NEAR((ReadSteadyClock() - start) / 1ms, 1000, 100);
  sr.Close();
}

TEST(StreamWriter, Expiration) {
  StreamWriter<int> sw(MakeRefCounted<ExpiringWriterProvider>());
  sw.SetExpiration(Later(1s));
  auto start = ReadSteadyClock();
  sw.Write(1);
  ASSERT_NEAR((ReadSteadyClock() - start) / 1ms, 1000, 100);
  sw.Close();
}

}  // namespace flare

FLARE_TEST_MAIN
