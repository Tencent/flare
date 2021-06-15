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

#include "flare/rpc/internal/buffered_stream_provider.h"

#include "gtest/gtest.h"

#include "flare/base/maybe_owning.h"
#include "flare/testing/main.h"

using namespace std::literals;

namespace flare::rpc::detail {

TEST(BufferedStreamReaderProvider, Timeout) {
  auto srp = MakeRefCounted<BufferedStreamReaderProvider<int>>(
      10, [] {}, [] {}, [] {});
  StreamReader<int> sr(srp);
  auto expected = ReadSteadyClock() + 100ms;
  sr.SetExpiration(expected);
  ASSERT_FALSE(sr.Read());
  ASSERT_NEAR(expected.time_since_epoch() / 1ms,
              ReadSteadyClock().time_since_epoch() / 1ms, 10);
}

TEST(BufferedStreamReaderProvider, FalseTimeout) {
  auto srp = MakeRefCounted<BufferedStreamReaderProvider<int>>(
      10, [] {}, [] {}, [] {});
  StreamReader<int> sr(srp);
  auto expected = ReadSteadyClock() + 100ms;
  sr.SetExpiration(expected);
  sr.Close();  // No leak even if the timer is not fired.
}

TEST(BufferedStreamReaderProvider, All) {
  int consumed = 0;
  bool closed = false;
  bool cleaned_up = false;
  auto srp = MakeRefCounted<BufferedStreamReaderProvider<int>>(
      10, [&] { ++consumed; }, [&] { closed = true; },
      [&] { cleaned_up = true; });
  StreamReader<int> sr(srp);
  srp->OnDataAvailable(10);
  ASSERT_EQ(10, **sr.Peek());
  ASSERT_EQ(10, *sr.Read());
  ASSERT_EQ(1, consumed);
  srp->OnDataAvailable(StreamError::EndOfStream);
  ASSERT_FALSE(sr.Read());
  ASSERT_EQ(2, consumed);  // Does not make much sense.
  ASSERT_TRUE(closed);
  ASSERT_TRUE(cleaned_up);
}

TEST(BufferedStreamReaderProvider, ReadEofCloseCleanupOrder) {
  bool closed = false;
  bool cb_called = false;
  auto srp = MakeRefCounted<BufferedStreamReaderProvider<int>>(
      10, [] {}, [&] { closed = true; }, [] {});
  AsyncStreamReader<int> reader(srp);
  srp->OnDataAvailable(StreamError::EndOfStream);
  reader.Read().Then([&](auto e) {
    ASSERT_EQ(StreamError::EndOfStream, e.error());
    ASSERT_TRUE(closed);  // Called before user's cb.
    cb_called = true;
  });
  ASSERT_TRUE(cb_called);
  ASSERT_TRUE(closed);
}

TEST(BufferedStreamWriterProvider, WriteLast) {
  int written = 0;
  bool closed = false;

  BufferedStreamWriterProvider<int>* swp_ptr;
  auto write_cb = [&](int v) {
    written = v;
    swp_ptr->OnWriteCompletion(true);
  };

  auto swp = MakeRefCounted<BufferedStreamWriterProvider<int>>(
      10, write_cb, [&] { closed = true; }, [] {});
  StreamWriter<int> sw(swp);
  swp_ptr = swp.Get();

  sw.WriteLast(10);
  ASSERT_EQ(10, written);
  ASSERT_TRUE(closed);
}

TEST(BufferedStreamWriterProvider, Timeout) {
  auto swp = MakeRefCounted<BufferedStreamWriterProvider<int>>(
      1, [](int) {}, [] {}, [] {});
  StreamWriter<int> sw(swp);
  auto expected = ReadSteadyClock() + 100ms;
  sw.SetExpiration(expected);
  ASSERT_FALSE(sw.Write(10));
  sw.Close();
  ASSERT_NEAR(expected.time_since_epoch() / 1ms,
              ReadSteadyClock().time_since_epoch() / 1ms, 10);
}

TEST(BufferedStreamWriterProvider, WriteAndClose) {
  int written = 0;
  bool closed = false;

  BufferedStreamWriterProvider<int>* swp_ptr;
  auto write_cb = [&](int v) {
    written = v;
    swp_ptr->OnWriteCompletion(true);
  };

  auto swp = MakeRefCounted<BufferedStreamWriterProvider<int>>(
      10, write_cb, [&] { closed = true; }, [] {});
  StreamWriter<int> sw(swp);
  swp_ptr = swp.Get();

  sw.Write(10);
  ASSERT_EQ(10, written);
  ASSERT_FALSE(closed);
  sw.Close();
  ASSERT_TRUE(closed);
}

TEST(BufferedStreamWriterProvider, CloseCleanupOrder) {
  bool closed = false;
  bool cb_called = false;
  auto srp = MakeRefCounted<BufferedStreamWriterProvider<int>>(
      10, [](auto) {}, [&] { closed = true; }, [] {});
  AsyncStreamWriter<int> writer(srp);
  writer.Close().Then([&] {
    ASSERT_TRUE(closed);
    cb_called = true;
  });
  ASSERT_TRUE(cb_called);
  ASSERT_TRUE(closed);
}

TEST(BufferedStreamWriterProvider, WriteLastCloseCleanupOrder) {
  bool closed = false;
  bool cb_called = false;
  auto srp = MakeRefCounted<BufferedStreamWriterProvider<int>>(
      10, [](auto) {}, [&] { closed = true; }, [] {});
  AsyncStreamWriter<int> writer(srp);
  writer.WriteLast(1).Then([&](bool) {
    ASSERT_TRUE(closed);
    cb_called = true;
  });
  ASSERT_FALSE(cb_called);
  srp->OnWriteCompletion(true);
  ASSERT_TRUE(cb_called);
  ASSERT_TRUE(closed);
}

}  // namespace flare::rpc::detail

FLARE_TEST_MAIN
