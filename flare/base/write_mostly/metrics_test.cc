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

#include <thread>

#include "googletest/gtest/gtest.h"

#include "flare/testing/main.h"

using namespace std::literals;

namespace flare {

namespace write_mostly::detail {

TEST(MetricsWriteMostly, Basic) {
  MetricsWriteMostly<int> a;
  a.Update(MetricsStats(1));
  a.Update(MetricsStats<int>());
  a.Update(MetricsStats(10, 2, 5, 4));
  auto&& read = a.Read();
  EXPECT_EQ(11, read.sum);
  EXPECT_EQ(3, read.cnt);
  EXPECT_EQ(5, read.max);
  EXPECT_EQ(1, read.min);
}

template <class T, class F>
void UpdateLoopWithOpTest(std::size_t num_threads, std::size_t num_loops,
                          std::size_t num_op, std::function<void(T&)> op,
                          F&& check_func) {
  std::vector<std::thread> threads;
  T adder;
  for (std::size_t i = 1; i <= num_threads; ++i) {
    threads.emplace_back([&, i] {
      for (std::size_t j = 0; j < num_loops * i; ++j) {
        adder.Update(MetricsStats(1));
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

TEST(MetricsWriteMostly, Read) {
  AtomicMetricsStats<int> total;
  UpdateLoopWithOpTest<MetricsWriteMostly<int>>(
      16, 100000, 10, [&](auto&& adder) { adder.Read(); },
      [&total](auto&& final_val) {
        total.Update(final_val);
        EXPECT_EQ(13600000, total.metrics_stats.cnt);
      });
}

TEST(MetricsWriteMostly, Purge) {
  AtomicMetricsStats<int> total;
  UpdateLoopWithOpTest<MetricsWriteMostly<int>>(
      16, 100000, 10, [&](auto&& adder) { total.Update(adder.Purge()); },
      [&total](auto&& final_val) {
        total.Update(final_val);
        EXPECT_EQ(13600000, total.metrics_stats.cnt);
      });
}

}  // namespace write_mostly::detail

TEST(MetricsTest, Basic) {
  WriteMostlyMetrics<int> m;
  m.Report(1);
  m.Report(3);
  std::this_thread::sleep_for(2s);
  auto&& result = m.Get(3);
  EXPECT_EQ(WriteMostlyMetrics<int>::Result({1, 3, 2, 2}), result);
  result = m.Get(3600);
  EXPECT_EQ(WriteMostlyMetrics<int>::Result({1, 3, 2, 2}), result);

  // BUG: We don't wait for timer to be fully stopped at the moment. This can
  // possibly lead to use-after-free (if `m` is destroyed when the timer is
  // running). Sleeping for a while workaround this. TODO(luobogao): Fix it.
  std::this_thread::sleep_for(100ms);
}

TEST(MetricsTest, Get) {
  WriteMostlyMetrics<int> m;
  m.current_pos_ = 30;
  for (int i = 29; i >= 0; --i) {
    m.metrics_stats_records_[i] =
        std::make_unique<write_mostly::detail::MetricsStats<int>>(30 - i);
  }
  for (int i = 3599; i >= 2630; --i) {
    m.metrics_stats_records_[i] =
        std::make_unique<write_mostly::detail::MetricsStats<int>>(3630 - i);
  }
  m.total_ = write_mostly::detail::MetricsStats<int>({500500, 1001, 1000, 0});
  EXPECT_EQ(m.Get(1), WriteMostlyMetrics<int>::Result({1, 1, 1, 1}));
  EXPECT_EQ(m.Get(60), WriteMostlyMetrics<int>::Result({1, 60, 30, 60}));
  EXPECT_EQ(m.Get(3600), WriteMostlyMetrics<int>::Result({1, 1000, 500, 1000}));
  EXPECT_EQ(m.GetAll(), WriteMostlyMetrics<int>::Result({0, 1000, 500, 1001}));
}

}  // namespace flare

FLARE_TEST_MAIN
