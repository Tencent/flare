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

#include "flare/fiber/fiber.h"

#include <atomic>
#include <thread>
#include <vector>

#include "gflags/gflags.h"
#include "gtest/gtest.h"

#include "flare/base/internal/cpu.h"
#include "flare/base/random.h"
#include "flare/fiber/fiber_local.h"
#include "flare/fiber/runtime.h"
#include "flare/fiber/this_fiber.h"

DECLARE_bool(flare_fiber_stack_enable_guard_page);
DECLARE_int32(flare_cross_numa_work_stealing_ratio);
DECLARE_int32(flare_fiber_run_queue_size);

using namespace std::literals;

namespace flare {

namespace {

template <class F>
void RunAsFiber(F&& f) {
  fiber::StartRuntime();
  std::atomic<bool> done{};
  Fiber([&, f = std::forward<F>(f)] {
    f();
    done = true;
  }).detach();
  while (!done) {
    std::this_thread::sleep_for(1ms);
  }
  fiber::TerminateRuntime();
}

}  // namespace

TEST(Fiber, StartWithDispatch) {
  FLAGS_flare_fiber_stack_enable_guard_page = false;
  FLAGS_flare_fiber_run_queue_size = 1048576;

  RunAsFiber([] {
    for (int k = 0; k != 10; ++k) {
      constexpr auto N = 10000;

      std::atomic<std::size_t> run{};
      std::vector<Fiber> fs(N);

      for (int i = 0; i != N; ++i) {
        fs[i] = Fiber([&] {
          auto we_re_in = std::this_thread::get_id();
          Fiber(fiber::Launch::Dispatch, [&, we_re_in] {
            ASSERT_EQ(we_re_in, std::this_thread::get_id());
            ++run;
          }).detach();
        });
      }

      while (run != N) {
        std::this_thread::sleep_for(1ms);
      }

      for (auto&& e : fs) {
        ASSERT_TRUE(e.joinable());
        e.join();
      }

      ASSERT_EQ(N, run);
    }
  });
}

TEST(Fiber, SchedulingGroupLocal) {
  FLAGS_flare_fiber_stack_enable_guard_page = false;
  FLAGS_flare_fiber_run_queue_size = 1048576;

  RunAsFiber([] {
    constexpr auto N = 10000;

    std::atomic<std::size_t> run{};
    std::vector<Fiber> fs(N);
    std::atomic<bool> stop{};

    for (std::size_t i = 0; i != N; ++i) {
      auto sgi = i % fiber::GetSchedulingGroupCount();
      auto cb = [&, sgi] {
        while (!stop) {
          ASSERT_EQ(sgi, fiber::detail::NearestSchedulingGroupIndex());
          this_fiber::Yield();
        }
        ++run;
      };
      fs[i] = Fiber(Fiber::Attributes{.scheduling_group = sgi,
                                      .scheduling_group_local = true},
                    cb);
    }

    auto start = ReadSteadyClock();

    // 10s should be far from enough. In my test the assertion above fires
    // almost immediately if `scheduling_group_local` is not set.
    while (start + 10s > ReadSteadyClock()) {
      std::this_thread::sleep_for(1ms);

      // Wake up workers in each scheduling group (for them to be thieves.).
      auto count = fiber::GetSchedulingGroupCount();
      for (std::size_t i = 0; i != count; ++i) {
        Fiber(Fiber::Attributes{.scheduling_group = i}, [] {}).join();
      }
    }

    stop = true;
    for (auto&& e : fs) {
      ASSERT_TRUE(e.joinable());
      e.join();
    }

    ASSERT_EQ(N, run);
  });
}

TEST(Fiber, WorkStealing) {
  if (internal::numa::GetAvailableNodes().size() == 1) {
    FLARE_LOG_INFO("Non-NUMA system, ignored.");
    return;
  }
  FLAGS_flare_fiber_stack_enable_guard_page = false;
  FLAGS_flare_cross_numa_work_stealing_ratio = 1;

  RunAsFiber([] {
    std::atomic<bool> stealing_happened{};
    constexpr auto N = 10000;

    std::atomic<std::size_t> run{};
    std::vector<Fiber> fs(N);

    for (int i = 0; i != N; ++i) {
      Fiber::Attributes attr{.scheduling_group =
                                 i % fiber::GetSchedulingGroupCount()};
      auto cb = [&] {
        auto start_node = internal::numa::GetCurrentNode();
        while (!stealing_happened) {
          if (start_node != internal::numa::GetCurrentNode()) {
            FLARE_LOG_INFO("Start on node #{}, running on node #{} now.",
                           start_node, internal::numa::GetCurrentNode());
            stealing_happened = true;
          } else {
            this_fiber::SleepFor(1us);
          }
        }
        ++run;
      };
      fs[i] = Fiber(attr, cb);
    }

    while (run != N) {
      std::this_thread::sleep_for(1ms);

      auto count = fiber::GetSchedulingGroupCount();
      for (std::size_t i = 0; i != count; ++i) {
        Fiber(Fiber::Attributes{.scheduling_group = i}, [] {}).join();
      }
    }

    for (auto&& e : fs) {
      ASSERT_TRUE(e.joinable());
      e.join();
    }

    ASSERT_EQ(N, run);
    ASSERT_TRUE(stealing_happened);
  });
}

TEST(Fiber, BatchStart) {
  RunAsFiber([&] {
    static constexpr auto N = 1000;
    static constexpr auto B = 10000;
    std::atomic<std::size_t> started{};

    for (int i = 0; i != N; ++i) {
      std::atomic<std::size_t> done{};
      std::vector<Function<void()>> procs;
      for (int j = 0; j != B; ++j) {
        procs.push_back([&] {
          ++started;
          ++done;
        });
      }
      fiber::internal::BatchStartFiberDetached(std::move(procs));
      while (done != B) {
      }
    }

    ASSERT_EQ(N * B, started.load());
  });
}

TEST(Fiber, StartFiberFromPthread) {
  RunAsFiber([&] {
    std::atomic<bool> called{};
    std::thread([&] {
      StartFiberFromPthread([&] {
        this_fiber::Yield();  // Would crash in pthread.
        ASSERT_TRUE(1);
        called = true;
      });
    }).join();
    while (!called.load()) {
      // NOTHING.
    }
  });
}

}  // namespace flare
