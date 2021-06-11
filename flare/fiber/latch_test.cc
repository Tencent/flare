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

#include "flare/fiber/latch.h"

#include <atomic>
#include <memory>
#include <thread>

#include "googletest/gtest/gtest.h"

#include "flare/base/chrono.h"
#include "flare/fiber/detail/testing.h"
#include "flare/fiber/fiber.h"
#include "flare/fiber/this_fiber.h"

using namespace std::literals;

namespace flare::fiber {

std::atomic<bool> exiting{false};

void RunTest() {
  std::atomic<std::size_t> local_count = 0, remote_count = 0;
  while (!exiting) {
    Latch l(1);
    auto called = std::make_shared<std::atomic<bool>>(false);
    Fiber([called, &l, &remote_count] {
      if (!called->exchange(true)) {
        this_fiber::Yield();
        l.count_down();
        ++remote_count;
      }
    }).detach();
    this_fiber::Yield();
    if (!called->exchange(true)) {
      l.count_down();
      ++local_count;
    }
    l.wait();
  }
  std::cout << local_count << " " << remote_count << std::endl;
}

TEST(Latch, Torture) {
  testing::RunAsFiber([] {
    Fiber fs[10];
    for (auto&& f : fs) {
      f = Fiber(RunTest);
    }
    std::this_thread::sleep_for(10s);
    exiting = true;
    for (auto&& f : fs) {
      f.join();
    }
  });
}

TEST(Latch, CountDownTwo) {
  testing::RunAsFiber([] {
    Latch l(2);
    l.arrive_and_wait(2);
    ASSERT_TRUE(1);
  });
}

TEST(Latch, WaitFor) {
  testing::RunAsFiber([] {
    Latch l(1);
    ASSERT_FALSE(l.wait_for(1s));
    l.count_down();
    ASSERT_TRUE(l.wait_for(0ms));
  });
}

TEST(Latch, WaitUntil) {
  testing::RunAsFiber([] {
    Latch l(1);
    ASSERT_FALSE(l.wait_until(ReadSteadyClock() + 1s));
    l.count_down();
    ASSERT_TRUE(l.wait_until(ReadSteadyClock()));
  });
}

}  // namespace flare::fiber
