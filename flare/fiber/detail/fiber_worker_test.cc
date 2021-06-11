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

#include "flare/fiber/detail/fiber_worker.h"

#include <deque>
#include <memory>
#include <thread>
#include <vector>

#include "gflags/gflags.h"
#include "googletest/gtest/gtest.h"

#include "flare/base/internal/cpu.h"
#include "flare/base/random.h"
#include "flare/fiber/detail/fiber_entity.h"
#include "flare/fiber/detail/scheduling_group.h"
#include "flare/fiber/detail/testing.h"
#include "flare/fiber/detail/timer_worker.h"

using namespace std::literals;

namespace flare::fiber::detail {

class SystemFiberOrNot : public ::testing::TestWithParam<bool> {};

TEST_P(SystemFiberOrNot, Affinity) {
  for (int k = 0; k != 1000; ++k) {
    auto sg = std::make_unique<SchedulingGroup>(std::vector<int>{1, 2, 3}, 16);
    TimerWorker dummy(sg.get());
    sg->SetTimerWorker(&dummy);
    std::deque<FiberWorker> workers;

    for (int i = 0; i != 16; ++i) {
      workers.emplace_back(sg.get(), i).Start(false);
    }
    testing::StartFiberEntityInGroup(sg.get(), GetParam(), [&] {
      auto cpu = flare::internal::GetCurrentProcessorId();
      ASSERT_TRUE(1 <= cpu && cpu <= 3);
    });
    sg->Stop();
    for (auto&& w : workers) {
      w.Join();
    }
  }
}

TEST_P(SystemFiberOrNot, ExecuteFiber) {
  std::atomic<std::size_t> executed{0};
  auto sg = std::make_unique<SchedulingGroup>(std::vector<int>{1, 2, 3}, 16);
  TimerWorker dummy(sg.get());
  sg->SetTimerWorker(&dummy);
  std::deque<FiberWorker> workers;

  for (int i = 0; i != 16; ++i) {
    workers.emplace_back(sg.get(), i).Start(false);
  }
  testing::StartFiberEntityInGroup(sg.get(), GetParam(), [&] {
    auto cpu = flare::internal::GetCurrentProcessorId();
    ASSERT_TRUE(1 <= cpu && cpu <= 3);
    ++executed;
  });
  sg->Stop();
  for (auto&& w : workers) {
    w.Join();
  }

  ASSERT_EQ(1, executed);
}

TEST_P(SystemFiberOrNot, StealFiber) {
  std::atomic<std::size_t> executed{0};
  auto sg = std::make_unique<SchedulingGroup>(std::vector<int>{1, 2, 3}, 16);
  auto sg2 = std::make_unique<SchedulingGroup>(std::vector<int>{}, 1);
  TimerWorker dummy(sg.get());
  sg->SetTimerWorker(&dummy);
  std::deque<FiberWorker> workers;

  testing::StartFiberEntityInGroup(sg2.get(), GetParam(), [&] { ++executed; });
  for (int i = 0; i != 16; ++i) {
    auto&& w = workers.emplace_back(sg.get(), i);
    w.AddForeignSchedulingGroup(sg2.get(), 1);
    w.Start(false);
  }
  while (!executed) {
    // To wake worker up.
    testing::StartFiberEntityInGroup(sg.get(), GetParam(), [] {});
    std::this_thread::sleep_for(1ms);
  }
  sg->Stop();
  for (auto&& w : workers) {
    w.Join();
  }

  ASSERT_EQ(1, executed);
}

INSTANTIATE_TEST_SUITE_P(FiberWorker, SystemFiberOrNot,
                         ::testing::Values(true, false));

TEST(FiberWorker, Torture) {
  constexpr auto T = 64;
  // Setting it too large cause `vm.max_map_count` overrun.
  constexpr auto N = 32768;
  constexpr auto P = 128;
  for (int i = 0; i != 50; ++i) {
    std::atomic<std::size_t> executed{0};
    auto sg = std::make_unique<SchedulingGroup>(std::vector<int>{}, T);
    TimerWorker dummy(sg.get());
    sg->SetTimerWorker(&dummy);
    std::deque<FiberWorker> workers;

    for (int i = 0; i != T; ++i) {
      workers.emplace_back(sg.get(), i).Start(false);
    }

    // Concurrently create fibers.
    std::thread prods[P];
    for (auto&& t : prods) {
      t = std::thread([&] {
        constexpr auto kChildren = 32;
        static_assert(N % P == 0 && (N / P) % kChildren == 0);
        for (int i = 0; i != N / P / kChildren; ++i) {
          testing::StartFiberEntityInGroup(
              sg.get(), Random() % 2 == 0 /* system_fiber */, [&] {
                ++executed;
                for (auto j = 0; j != kChildren - 1 /* minus itself */; ++j) {
                  testing::StartFiberEntityInGroup(
                      sg.get(), Random() % 2 == 0 /* system_fiber */,
                      [&] { ++executed; });
                }
              });
        }
      });
    }

    for (auto&& t : prods) {
      t.join();
    }
    while (executed != N) {
      std::this_thread::sleep_for(100ms);
    }
    sg->Stop();
    for (auto&& w : workers) {
      w.Join();
    }

    ASSERT_EQ(N, executed);
  }
}

}  // namespace flare::fiber::detail
