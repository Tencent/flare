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

#include "flare/fiber/fiber_local.h"

#include <atomic>
#include <thread>
#include <vector>

#include "thirdparty/googletest/gmock/gmock-matchers.h"
#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/base/random.h"
#include "flare/fiber/detail/testing.h"
#include "flare/fiber/fiber.h"
#include "flare/fiber/this_fiber.h"

using namespace std::literals;

namespace flare {

TEST(FiberLocal, All) {
  for (int k = 0; k != 10; ++k) {
    fiber::testing::RunAsFiber([] {
      static FiberLocal<int> fls;
      static FiberLocal<int> fls2;
      static FiberLocal<double> fls3;
      static FiberLocal<std::vector<int>> fls4;
      constexpr auto N = 10000;

      std::atomic<std::size_t> run{};
      std::vector<Fiber> fs(N);

      for (int i = 0; i != N; ++i) {
        fs[i] = Fiber([i, &run] {
          *fls = i;
          *fls2 = i * 2;
          *fls3 = i + 3;
          fls4->push_back(123);
          fls4->push_back(456);
          while (Random(20) > 1) {
            this_fiber::SleepFor(Random(1000) * 1us);
            ASSERT_EQ(i, *fls);
            ASSERT_EQ(i * 2, *fls2);
            ASSERT_EQ(i + 3, *fls3);
            ASSERT_THAT(*fls4, ::testing::ElementsAre(123, 456));
          }
          ++run;
        });
      }

      for (auto&& e : fs) {
        ASSERT_TRUE(e.joinable());
        e.join();
      }

      ASSERT_EQ(N, run);
    });
  }
}

}  // namespace flare
