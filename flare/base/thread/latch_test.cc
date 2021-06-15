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

#include "flare/base/thread/latch.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include "gtest/gtest.h"

#include "flare/base/chrono.h"

using namespace std::literals;

namespace flare {

std::atomic<bool> exiting{false};

void RunTest() {
  std::size_t local_count = 0, remote_count = 0;
  while (!exiting) {
    auto called = std::make_shared<std::atomic<bool>>(false);
    std::this_thread::yield();  // Wait for thread pool to start.
    Latch l(1);
    auto t = std::thread([&] {
      if (!called->exchange(true)) {
        std::this_thread::yield();  // Something costly.
        l.count_down();
        ++remote_count;
      }
    });
    std::this_thread::yield();  // Something costly.
    if (!called->exchange(true)) {
      l.count_down();
      ++local_count;
    }
    l.wait();
    t.join();
  }
  std::cout << local_count << " " << remote_count << std::endl;
}

TEST(Latch, Torture) {
  std::thread ts[10];
  for (auto&& t : ts) {
    t = std::thread(RunTest);
  }
  std::this_thread::sleep_for(10s);
  exiting = true;
  for (auto&& t : ts) {
    t.join();
  }
}

TEST(Latch, CountDownTwo) {
  Latch l(2);
  l.arrive_and_wait(2);
  ASSERT_TRUE(1);
}

TEST(Latch, WaitFor) {
  Latch l(1);
  ASSERT_FALSE(l.wait_for(100ms));
  l.count_down();
  ASSERT_TRUE(l.wait_for(0ms));
}

TEST(Latch, WaitUntil) {
  Latch l(1);
  ASSERT_FALSE(l.wait_until(ReadSteadyClock() + 100ms));
  l.count_down();
  ASSERT_TRUE(l.wait_until(ReadSteadyClock()));
}

}  // namespace flare
