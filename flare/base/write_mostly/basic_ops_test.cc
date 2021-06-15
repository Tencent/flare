// Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "flare/base/write_mostly.h"

#include <atomic>
#include <vector>

#include "gtest/gtest.h"

namespace flare {

template <class T, class Op>
void BasicTest(T result) {
  Op t;
  t.Update(T(1));
  t.Update(T(2));
  t.Update(T(3));
  EXPECT_EQ(result, t.Read());
  t.Reset();
}

TEST(BasicOps, Basic) {
  WriteMostlyCounter<int> counter;
  counter.Add(1);
  counter.Add(2);
  counter.Add(3);
  EXPECT_EQ(6, counter.Read());
  counter.Reset();
}

TEST(BasicOps, Basic2) {
  WriteMostlyGauge<int> counter;
  counter.Add(1);
  counter.Add(2);
  counter.Subtract(4);
  EXPECT_EQ(-1, counter.Read());
  counter.Reset();
}

TEST(BasicOps, Basic3) {
  BasicTest<int, WriteMostlyMiner<int>>(1);
  BasicTest<int, WriteMostlyMaxer<int>>(3);
  BasicTest<int, WriteMostlyAverager<int>>(2);
}

template <class T>
void UpdateLoopWithOpTest(std::size_t num_threads, std::size_t num_loops,
                          std::size_t num_op, std::function<void(T&)> op,
                          std::function<void(int)> check_func) {
  std::vector<std::thread> threads;
  T adder;
  for (std::size_t i = 1; i <= num_threads; ++i) {
    threads.emplace_back([&, i] {
      for (std::size_t j = 0; j < num_loops * i; ++j) {
        adder.Add(1);
        if (j * num_op / num_loops == 0) {
          op(adder);
        }
      }
    });
  }
  for (auto& t : threads) {
    t.join();
  }
  check_func(adder.Read());
}

TEST(WriteMostly, Read) {
  UpdateLoopWithOpTest<WriteMostlyCounter<int>>(
      16, 100000, 10, [](auto&& adder) { adder.Read(); },
      [](auto&& final_val) { EXPECT_EQ(13600000, final_val); });
}

TEST(WriteMostly, Reset) {
  UpdateLoopWithOpTest<WriteMostlyCounter<int>>(
      16, 100000, 10, [](auto&& adder) { adder.Reset(); },
      [](auto&& final_val) { EXPECT_GT(13600000, final_val); });
}

template <class T>
struct AtomicAdderTraits {
  using Type = T;
  using WriteBuffer = T;
  static constexpr auto kWriteBufferInitializer = T();
  static void Update(WriteBuffer* wb, const T& val) { *wb += val; }
  static void Merge(WriteBuffer* wb1, const WriteBuffer& wb2) { *wb1 += wb2; }
  static void Copy(const WriteBuffer& src_wb, WriteBuffer* dst_wb) {
    *dst_wb = src_wb.load();
  }
  static T Read(const WriteBuffer& wb) { return wb.load(); }
  static WriteBuffer Purge(WriteBuffer* wb) {
    return wb->exchange(kWriteBufferInitializer);
  }
};

template <class T>
class AtomicAdder : public WriteMostly<AtomicAdderTraits<T>> {
 public:
  void Add(const T& value) { WriteMostly<AtomicAdderTraits<T>>::Update(value); }
};

TEST(WriteMostly, Purge) {
  std::atomic<int> total(0);
  UpdateLoopWithOpTest<AtomicAdder<std::atomic<int>>>(
      16, 100000, 10, [&](auto&& adder) { total += adder.Purge(); },
      [&total](auto&& final_val) { EXPECT_EQ(13600000, total + final_val); });
}

}  // namespace flare
