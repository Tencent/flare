// Copyright (C) 2022 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "flare/fiber/barrier.h"

#include "gtest/gtest.h"

#include "flare/fiber/detail/testing.h"

namespace flare::fiber {

TEST(Barrier, ArriveAndWait) {
  testing::RunAsFiber([] {
    bool i = false;
    Barrier b(1, [&] { i = true; });
    b.arrive_and_wait();
    ASSERT_TRUE(i);
    i = false;
    b.arrive_and_wait();
    ASSERT_TRUE(i);
  });
}

TEST(Barrier, ArriveAndDrop) {
  testing::RunAsFiber([] {
    int n = 0;
    Barrier b(1, [&n] { ++n; });
    b.arrive_and_drop();
    ASSERT_EQ(n, 1);
  });
}

// Same as https://en.cppreference.com/w/cpp/thread/barrier example
TEST(Barrier, Simple) {
  testing::RunAsFiber([] {
    const auto workers = {"anil", "busara", "carl"};

    auto on_completion = []() noexcept {
      // locking not needed here
      static auto phase =
          "... done\n"
          "Cleaning up...\n";
      std::cout << phase;
      phase = "... done\n";
    };
    Barrier sync_point(workers.size(), on_completion);

    auto work = [&](std::string name) {
      std::string product = "  " + name + " worked\n";
      std::cout << product;  // ok, op<< call is atomic
      sync_point.arrive_and_wait();

      product = "  " + name + " cleaned\n";
      std::cout << product;
      sync_point.arrive_and_wait();
    };

    std::cout << "Starting...\n";
    std::vector<Fiber> fibers;
    for (auto const& worker : workers) {
      fibers.emplace_back(work, worker);
    }
    for (auto& fiber : fibers) {
      fiber.join();
    }
  });
}

}  // namespace flare::fiber
