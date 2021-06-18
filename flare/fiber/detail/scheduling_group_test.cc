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

#include "flare/fiber/detail/scheduling_group.h"

#include <algorithm>
#include <fstream>
#include <string>
#include <thread>

#include "gflags/gflags.h"
#include "gtest/gtest.h"

#include "flare/base/chrono.h"
#include "flare/base/random.h"
#include "flare/base/string.h"
#include "flare/fiber/detail/fiber_entity.h"
#include "flare/fiber/detail/testing.h"
#include "flare/fiber/detail/timer_worker.h"
#include "flare/fiber/detail/waitable.h"

DECLARE_int32(flare_fiber_run_queue_size);

using namespace std::literals;

namespace flare::fiber::detail {

namespace {

template <class SG, class... Args>
[[nodiscard]] std::uint64_t SetTimerAt(SG&& sg, Args&&... args) {
  auto tid = sg->CreateTimer(std::forward<Args>(args)...);
  sg->EnableTimer(tid);
  return tid;
}

std::size_t GetMaxFibers() {
  std::ifstream ifs("/proc/sys/vm/max_map_count");
  std::string s;
  std::getline(ifs, s);
  return std::min<std::size_t>(TryParse<std::size_t>(s).value_or(65530) / 4,
                               131072);
}

FiberEntity* CreateFiberEntity(SchedulingGroup* sg, bool system_fiber,
                               Function<void()>&& start_proc) noexcept {
  auto desc = NewFiberDesc();
  desc->scheduling_group_local = false;
  desc->system_fiber = system_fiber;
  desc->start_proc = std::move(start_proc);
  return InstantiateFiberEntity(sg, desc);
}

}  // namespace

class SystemFiberOrNot : public ::testing::TestWithParam<bool> {};

TEST(SchedulingGroup, Create) {
  // Hopefully we have at least 4 cores on machine running UT.
  auto scheduling_group =
      std::make_unique<SchedulingGroup>(std::vector<int>{0, 1, 2, 3}, 20);

  // No affinity applied.
  auto scheduling_group2 =
      std::make_unique<SchedulingGroup>(std::vector<int>{}, 20);
}

void WorkerTest(SchedulingGroup* sg, std::size_t index) {
  sg->EnterGroup(index);

  while (true) {
    FiberEntity* ready = sg->AcquireFiber();

    while (ready == nullptr) {
      ready = sg->WaitForFiber();
    }
    if (ready == SchedulingGroup::kSchedulingGroupShuttingDown) {
      break;
    }
    ready->Resume();
    ASSERT_EQ(GetCurrentFiberEntity(), GetMasterFiberEntity());
  }
  sg->LeaveGroup();
}

struct Context {
  std::atomic<std::size_t> executed_fibers{0};
  std::atomic<std::size_t> yields{0};
} context;

void FiberProc(Context* ctx) {
  auto sg = SchedulingGroup::Current();
  auto self = GetCurrentFiberEntity();
  std::atomic<std::size_t> yield_count_local{0};

  // It can't be.
  ASSERT_NE(self, GetMasterFiberEntity());

  for (int i = 0; i != 10; ++i) {
    ASSERT_EQ(FiberState::Running, self->state);
    sg->Yield(self);
    ++ctx->yields;
    ASSERT_EQ(self, GetCurrentFiberEntity());
    ++yield_count_local;
  }

  // The fiber is resumed by two worker concurrently otherwise.
  ASSERT_EQ(10, yield_count_local);

  ++ctx->executed_fibers;
}

TEST_P(SystemFiberOrNot, RunFibers) {
  context.executed_fibers = 0;
  context.yields = 0;

  google::FlagSaver fs;
  FLAGS_flare_fiber_run_queue_size = 262144;

  static const auto N = GetMaxFibers();
  FLARE_LOG_INFO("Starting {} fibers.", N);

  auto scheduling_group =
      std::make_unique<SchedulingGroup>(std::vector<int>{}, 16);
  std::thread workers[16];
  TimerWorker dummy(scheduling_group.get());
  scheduling_group->SetTimerWorker(&dummy);

  for (int i = 0; i != 16; ++i) {
    workers[i] = std::thread(WorkerTest, scheduling_group.get(), i);
  }

  for (int i = 0; i != N; ++i) {
    testing::StartFiberEntityInGroup(scheduling_group.get(), GetParam(),
                                     [&] { FiberProc(&context); });
  }
  while (context.executed_fibers != N) {
    std::this_thread::sleep_for(100ms);
  }
  scheduling_group->Stop();
  for (auto&& t : workers) {
    t.join();
  }
  ASSERT_EQ(N, context.executed_fibers);
  ASSERT_EQ(N * 10, context.yields);
}

std::atomic<std::size_t> switched{};

void SwitchToNewFiber(SchedulingGroup* sg, bool system_fiber,
                      std::size_t left) {
  if (--left) {
    auto next = CreateFiberEntity(sg, system_fiber, [sg, system_fiber, left] {
      SwitchToNewFiber(sg, system_fiber, left);
    });
    sg->SwitchTo(GetCurrentFiberEntity(), next);
  }
  ++switched;
}

TEST_P(SystemFiberOrNot, SwitchToFiber) {
  switched = 0;
  google::FlagSaver fs;

  constexpr auto N = 16384;

  auto scheduling_group =
      std::make_unique<SchedulingGroup>(std::vector<int>{}, 16);
  std::thread workers[16];
  TimerWorker dummy(scheduling_group.get());
  scheduling_group->SetTimerWorker(&dummy);

  for (int i = 0; i != 16; ++i) {
    workers[i] = std::thread(WorkerTest, scheduling_group.get(), i);
  }

  testing::StartFiberEntityInGroup(scheduling_group.get(), GetParam(), [&] {
    SwitchToNewFiber(scheduling_group.get(), GetParam(), N);
  });
  while (switched != N) {
    std::this_thread::sleep_for(100ms);
  }
  scheduling_group->Stop();
  for (auto&& t : workers) {
    t.join();
  }
  ASSERT_EQ(N, switched);
}

TEST_P(SystemFiberOrNot, WaitForFiberExit) {
  google::FlagSaver fs;
  FLAGS_flare_fiber_run_queue_size = 262144;

  auto scheduling_group =
      std::make_unique<SchedulingGroup>(std::vector<int>{}, 16);
  std::thread workers[16];
  TimerWorker timer_worker(scheduling_group.get());
  scheduling_group->SetTimerWorker(&timer_worker);

  for (int i = 0; i != 16; ++i) {
    workers[i] = std::thread(WorkerTest, scheduling_group.get(), i);
  }
  timer_worker.Start();

  for (int k = 0; k != 100; ++k) {
    constexpr auto N = 1024;
    std::atomic<std::size_t> waited{};
    for (int i = 0; i != N; ++i) {
      auto f1 = CreateFiberEntity(
          scheduling_group.get(), Random() % 2 == 0 /* system_fiber */, [] {
            WaitableTimer wt(ReadCoarseSteadyClock() + Random(10) * 1ms);
            wt.wait();
          });
      f1->exit_barrier = object_pool::GetRefCounted<ExitBarrier>();
      auto f2 = CreateFiberEntity(scheduling_group.get(), GetParam(),
                                  [&, wc = f1->exit_barrier] {
                                    wc->Wait();
                                    ++waited;
                                  });
      if (Random() % 2 == 0) {
        scheduling_group->ReadyFiber(f1, {});
        scheduling_group->ReadyFiber(f2, {});
      } else {
        scheduling_group->ReadyFiber(f2, {});
        scheduling_group->ReadyFiber(f1, {});
      }
    }
    while (waited != N) {
      std::this_thread::sleep_for(10ms);
    }
  }
  scheduling_group->Stop();
  timer_worker.Stop();
  timer_worker.Join();
  for (auto&& t : workers) {
    t.join();
  }
}

void SleepyFiberProc(std::atomic<std::size_t>* leaving) {
  auto self = GetCurrentFiberEntity();
  auto sg = self->scheduling_group;
  std::unique_lock lk(self->scheduler_lock);

  auto timer_cb = [self](auto) {
    std::unique_lock lk(self->scheduler_lock);
    self->state = FiberState::Ready;
    self->scheduling_group->ReadyFiber(self, std::move(lk));
  };
  auto timer_id =
      SetTimerAt(sg, ReadSteadyClock() + 1s + Random(1000'000) * 1us, timer_cb);

  sg->Halt(self, std::move(lk));
  sg->RemoveTimer(timer_id);
  ++*leaving;
}

TEST_P(SystemFiberOrNot, SetTimer) {
  auto scheduling_group =
      std::make_unique<SchedulingGroup>(std::vector<int>{}, 16);
  std::thread workers[16];
  std::atomic<std::size_t> leaving{0};
  TimerWorker timer_worker(scheduling_group.get());
  scheduling_group->SetTimerWorker(&timer_worker);

  for (int i = 0; i != 16; ++i) {
    workers[i] = std::thread(WorkerTest, scheduling_group.get(), i);
  }
  timer_worker.Start();

  constexpr auto N = 30000;
  for (int i = 0; i != N; ++i) {
    testing::StartFiberEntityInGroup(scheduling_group.get(), GetParam(),
                                     [&] { SleepyFiberProc(&leaving); });
  }
  while (leaving != N) {
    std::this_thread::sleep_for(100ms);
  }
  scheduling_group->Stop();
  timer_worker.Stop();
  timer_worker.Join();
  for (auto&& t : workers) {
    t.join();
  }
  ASSERT_EQ(N, leaving);
}

TEST_P(SystemFiberOrNot, SetTimerPeriodic) {
  auto scheduling_group =
      std::make_unique<SchedulingGroup>(std::vector<int>{}, 1);
  TimerWorker timer_worker(scheduling_group.get());
  scheduling_group->SetTimerWorker(&timer_worker);
  auto worker = std::thread(WorkerTest, scheduling_group.get(), 0);
  timer_worker.Start();

  auto start = ReadSteadyClock();
  std::atomic<std::size_t> called{};
  std::atomic<std::uint64_t> timer_id;
  testing::StartFiberEntityInGroup(scheduling_group.get(), GetParam(), [&] {
    auto cb = [&](auto) {
      if (called < 10) {
        ++called;
      }
    };
    timer_id =
        SetTimerAt(scheduling_group, ReadSteadyClock() + 20ms, 100ms, cb);
  });
  while (called != 10) {
    std::this_thread::sleep_for(1ms);
  }
  ASSERT_NEAR((ReadSteadyClock() - start) / 1ms, (20ms + 100ms * 9) / 1ms, 10);
  scheduling_group->RemoveTimer(timer_id);
  scheduling_group->Stop();
  timer_worker.Stop();
  timer_worker.Join();
  worker.join();
  ASSERT_EQ(10, called);
}

INSTANTIATE_TEST_SUITE_P(SchedulingGroup, SystemFiberOrNot,
                         ::testing::Values(true, false));

}  // namespace flare::fiber::detail
