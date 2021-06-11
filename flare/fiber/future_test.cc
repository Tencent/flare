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

#include "flare/fiber/future.h"

#include <chrono>
#include <vector>

#include "googletest/gmock/gmock.h"
#include "googletest/gtest/gtest.h"

#include "flare/base/random.h"
#include "flare/fiber/async.h"
#include "flare/fiber/detail/testing.h"
#include "flare/fiber/runtime.h"
#include "flare/fiber/this_fiber.h"

using namespace std::literals;

namespace flare {

TEST(Future, BlockingGet) {
  fiber::testing::RunAsFiber([&] {
    for (int i = 0; i != 200; ++i) {
      Fiber fbs[100];
      for (auto&& f : fbs) {
        auto op = [] {
          auto v = fiber::BlockingGet(fiber::Async([] {
            std::vector<int> v{1, 2, 3, 4, 5};
            int round = Random(10);
            for (int j = 0; j != round; ++j) {
              this_fiber::Yield();
            }
            return v;
          }));
          ASSERT_THAT(v, ::testing::ElementsAre(1, 2, 3, 4, 5));
        };
        auto sg = Random() % fiber::GetSchedulingGroupCount();
        f = Fiber(Fiber::Attributes{.scheduling_group = sg}, op);
      }
      for (auto&& f : fbs) {
        f.join();
      }
    }
  });
}

TEST(Future, BlockingTryGetOk) {
  fiber::testing::RunAsFiber([&] {
    std::atomic<bool> f{};
    auto future = fiber::Async([&] {
      this_fiber::SleepFor(1s);
      f = true;
    });
    ASSERT_FALSE(fiber::BlockingTryGet(std::move(future), 10ms));
    ASSERT_FALSE(f);
    this_fiber::SleepFor(2s);
    ASSERT_TRUE(f);
  });
}

TEST(Future, BlockingTryGetTimeout) {
  fiber::testing::RunAsFiber([&] {
    auto future = fiber::Async([] {
      this_fiber::SleepFor(1s);
      return 10;
    });
    auto result = fiber::BlockingTryGet(std::move(future), 2s);
    ASSERT_TRUE(result);
    EXPECT_EQ(10, *result);
  });
}

}  // namespace flare
