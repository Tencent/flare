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

#include "flare/fiber/this_fiber.h"

#include <atomic>
#include <thread>
#include <vector>

#include "gflags/gflags.h"
#include "gtest/gtest.h"

#include "flare/base/random.h"
#include "flare/fiber/alternatives.h"
#include "flare/fiber/detail/testing.h"
#include "flare/fiber/fiber.h"
#include "flare/fiber/fiber_local.h"

using namespace std::literals;

DECLARE_bool(flare_fiber_stack_enable_guard_page);

namespace flare::this_fiber {

TEST(ThisFiber, Yield) {
  FLAGS_flare_fiber_stack_enable_guard_page = false;

  fiber::testing::RunAsFiber([] {
    for (int k = 0; k != 10; ++k) {
      constexpr auto N = 10000;

      std::atomic<std::size_t> run{};
      std::atomic<bool> ever_switched_thread{};
      std::vector<Fiber> fs(N);

      for (int i = 0; i != N; ++i) {
        fs[i] = Fiber([&run, &ever_switched_thread] {
          // `Yield()`
          auto tid = fiber::GetCurrentThreadId();
          while (tid == fiber::GetCurrentThreadId()) {
            this_fiber::Yield();
          }
          ever_switched_thread = true;
          ++run;
        });
      }

      for (auto&& e : fs) {
        ASSERT_TRUE(e.joinable());
        e.join();
      }

      ASSERT_EQ(N, run);
      ASSERT_TRUE(ever_switched_thread);
    }
  });
}

TEST(ThisFiber, Sleep) {
  fiber::testing::RunAsFiber([] {
    for (int k = 0; k != 10; ++k) {
      // Don't run too many fibers here, waking pthread worker up is costly and
      // incurs delay. With too many fibers, that delay fails the UT (we're
      // testing timer delay here).
      constexpr auto N = 100;

      std::atomic<std::size_t> run{};
      std::vector<Fiber> fs(N);

      for (int i = 0; i != N; ++i) {
        fs[i] = Fiber([&run] {
          // `SleepFor()`
          auto sleep_for = Random(100) * 1ms;
          auto start = ReadSystemClock();  // Used system_clock intentionally.
          this_fiber::SleepFor(sleep_for);
          ASSERT_NEAR((ReadSystemClock() - start) / 1ms, sleep_for / 1ms, 30);

          // `SleepUntil()`
          auto sleep_until = ReadSystemClock() + Random(100) * 1ms;
          this_fiber::SleepUntil(sleep_until);
          ASSERT_NEAR((ReadSystemClock().time_since_epoch()) / 1ms,
                      sleep_until.time_since_epoch() / 1ms, 30);

          ++run;
        });
      }

      for (auto&& e : fs) {
        ASSERT_TRUE(e.joinable());
        e.join();
      }

      ASSERT_EQ(N, run);
    }
  });
}

}  // namespace flare::this_fiber
