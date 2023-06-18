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

#include "flare/fiber/detail/timer_worker.h"

#include <memory>
#include <thread>
#include <utility>

#include "gtest/gtest.h"

#include "flare/base/chrono.h"
#include "flare/base/random.h"
#include "flare/base/thread/latch.h"
#include "flare/fiber/detail/scheduling_group.h"

using namespace std::literals;

namespace flare::fiber::detail {

namespace {

template <class SG, class... Args>
[[nodiscard]] std::uint64_t SetTimerAt(SG&& sg, Args&&... args) {
  auto tid = sg->CreateTimer(std::forward<Args>(args)...);
  sg->EnableTimer(tid);
  return tid;
}

}  // namespace

TEST(TimerWorker, EarlyTimer) {
  std::atomic<bool> called = false;

  auto scheduling_group =
      std::make_unique<SchedulingGroup>(std::vector<int>{1, 2, 3}, 1);
  TimerWorker worker(scheduling_group.get());
  scheduling_group->SetTimerWorker(&worker);
  std::thread t = std::thread([&scheduling_group, &called] {
    scheduling_group->EnterGroup(0);

    (void)SetTimerAt(scheduling_group,
                     std::chrono::steady_clock::time_point::min(),
                     [&](auto tid) {
                       scheduling_group->RemoveTimer(tid);
                       called = true;
                     });
    std::this_thread::sleep_for(1s);
    scheduling_group->LeaveGroup();
  });

  worker.Start();
  t.join();
  worker.Stop();
  worker.Join();

  ASSERT_TRUE(called);
}

TEST(TimerWorker, SetTimerInTimerContext) {
  std::atomic<bool> called = false;

  auto scheduling_group =
      std::make_unique<SchedulingGroup>(std::vector<int>{1, 2, 3}, 1);
  TimerWorker worker(scheduling_group.get());
  scheduling_group->SetTimerWorker(&worker);
  std::thread t = std::thread([&scheduling_group, &called] {
    scheduling_group->EnterGroup(0);

    auto timer_cb = [&](auto tid) {
      auto timer_cb2 = [&, tid](auto tid2) {
        scheduling_group->RemoveTimer(tid);
        scheduling_group->RemoveTimer(tid2);
        called = true;
      };
      (void)SetTimerAt(scheduling_group,
                       std::chrono::steady_clock::time_point{}, timer_cb2);
    };
    (void)SetTimerAt(scheduling_group,
                     std::chrono::steady_clock::time_point::min(), timer_cb);
    std::this_thread::sleep_for(1s);
    scheduling_group->LeaveGroup();
  });

  worker.Start();
  t.join();
  worker.Stop();
  worker.Join();

  ASSERT_TRUE(called);
}

std::atomic<std::size_t> timer_set, timer_removed;

TEST(TimerWorker, Torture) {
  constexpr auto N = 100000;
  constexpr auto T = 40;

  auto scheduling_group =
      std::make_unique<SchedulingGroup>(std::vector<int>{1, 2, 3}, T);
  TimerWorker worker(scheduling_group.get());
  scheduling_group->SetTimerWorker(&worker);
  std::thread ts[T];

  for (int i = 0; i != T; ++i) {
    ts[i] = std::thread([i, &scheduling_group] {
      scheduling_group->EnterGroup(i);

      for (int j = 0; j != N; ++j) {
        auto timeout = ReadCoarseSteadyClock() + Random(2'000'000) * 1us;
        if (j % 2 == 0) {
          // In this case we set a timer and let it fire.
          (void)SetTimerAt(scheduling_group, timeout,
                           [&scheduling_group](auto timer_id) {
                             scheduling_group->RemoveTimer(timer_id);
                             ++timer_removed;
                           });  // Indirectly calls `TimerWorker::AddTimer`.
          ++timer_set;
        } else {
          // In this case we set a timer and cancel it sometime later.
          auto timer_id = SetTimerAt(scheduling_group, timeout, [](auto) {});
          (void)SetTimerAt(scheduling_group,
                           ReadCoarseSteadyClock() + Random(1000) * 1ms,
                           [timer_id, &scheduling_group](auto self) {
                             scheduling_group->RemoveTimer(timer_id);
                             scheduling_group->RemoveTimer(self);
                             ++timer_removed;
                           });
          ++timer_set;
        }
        if (j % 10000 == 0) {
          std::this_thread::sleep_for(100ms);
        }
      }

      // Wait until all timers have been consumed. Otherwise if we leave the
      // thread too early, `TimerWorker` might incurs use-after-free when
      // accessing our thread-local timer queue.
      while (timer_removed.load(std::memory_order_relaxed) !=
             timer_set.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(100ms);
      }
      scheduling_group->LeaveGroup();
    });
  }

  worker.Start();

  for (auto&& t : ts) {
    t.join();
  }
  worker.Stop();
  worker.Join();

  ASSERT_EQ(timer_set, timer_removed);
  ASSERT_EQ(N * T, timer_set);
}

}  // namespace flare::fiber::detail
