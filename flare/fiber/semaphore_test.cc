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

#include "flare/fiber/semaphore.h"

#include <atomic>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

#include "flare/fiber/detail/testing.h"
#include "flare/fiber/fiber.h"

namespace flare::fiber {

std::atomic<int> counter{};

TEST(Semaphore, All) {
  testing::RunAsFiber([] {
    std::vector<Fiber> fibers;
    CountingSemaphore semaphore(100);

    for (int i = 0; i != 10000; ++i) {
      fibers.emplace_back([&] {
        semaphore.acquire();
        ++counter;
        EXPECT_LE(counter, 100);
        --counter;
        semaphore.release();
      });
    }
    for (auto&& e : fibers) {
      e.join();
    }
  });
}

}  // namespace flare::fiber
