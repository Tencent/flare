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

#include "flare/fiber/timer.h"

#include <atomic>
#include <thread>

#include "gtest/gtest.h"

#include "flare/fiber/detail/testing.h"
#include "flare/fiber/this_fiber.h"

using namespace std::literals;

namespace flare::fiber {

TEST(Timer, SetTimer) {
  testing::RunAsFiber([] {
    auto start = ReadSteadyClock();
    std::atomic<bool> done{};
    auto timer_id = SetTimer(start + 100ms, [&](auto) {
      ASSERT_NEAR((ReadSteadyClock() - start) / 1ms, 100ms / 1ms, 10);
      done = true;
    });
    while (!done) {
      std::this_thread::sleep_for(1ms);
    }
    KillTimer(timer_id);
  });
}

TEST(Timer, SetPeriodicTimer) {
  testing::RunAsFiber([] {
    auto start = ReadSteadyClock();
    std::atomic<std::size_t> called{};
    auto timer_id = SetTimer(start + 100ms, 10ms, [&](auto) {
      ASSERT_NEAR((ReadSteadyClock() - start) / 1ms,
                  (100ms + called.load() * 10ms) / 1ms, 10);
      ++called;
    });
    while (called != 10) {
      std::this_thread::sleep_for(1ms);
    }
    KillTimer(timer_id);

    // It's possible that the timer callback is running when `KillTimer` is
    // called, so wait for it to complete.
    std::this_thread::sleep_for(500ms);
  });
}

TEST(Timer, SetPeriodicTimerWithSlowCallback) {
  testing::RunAsFiber([] {
    auto start = ReadSteadyClock();
    std::atomic<std::size_t> called{};
    auto timer_id = SetTimer(start + 10ms, 10ms, [&](auto) {
      std::this_thread::sleep_for(100ms);  // Slower than timer interval.
      ++called;
    });
    std::this_thread::sleep_for(105ms);  // Fire the callback for 10 times.
    KillTimer(timer_id);

    // Each timer callback needs 100ms, and we're firing the callback 10 times,
    // so expect at least 1 second to elapse before our callback finishes.
    std::this_thread::sleep_for(1s + 100ms /* Tolerance to busy system. */);
    EXPECT_NEAR(10, called, 2 /* Tolerance to busy system. */);

    // It's possible that the timer callback is running when `KillTimer` is
    // called, so wait for it to complete. (Unlikely to happen except on
    // extremely busy system.)
    std::this_thread::sleep_for(500ms);
  });
}

TEST(Timer, TimerKiller) {
  testing::RunAsFiber([] {
    auto start = ReadSteadyClock();
    std::atomic<bool> done{};
    TimerKiller killer(SetTimer(start + 100ms, [&](auto) {
      ASSERT_NEAR((ReadSteadyClock() - start) / 1ms, 100ms / 1ms, 10);
      done = true;
    }));
    while (!done) {
      std::this_thread::sleep_for(1ms);
    }
    // We rely on heap checker here to ensure the timer is not leaked.
  });
}

TEST(Timer, SetDetachedTimer) {
  testing::RunAsFiber([] {
    auto start = ReadSteadyClock();
    std::atomic<bool> called{};
    SetDetachedTimer(start + 100ms, [&]() {
      ASSERT_NEAR((ReadSteadyClock() - start) / 1ms, 100ms / 1ms, 10);
      called = true;
    });
    while (!called) {
      std::this_thread::sleep_for(1ms);
    }
  });
  // No leak should be reported.
}

}  // namespace flare::fiber
