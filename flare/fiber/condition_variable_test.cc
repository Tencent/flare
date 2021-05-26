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

#include "flare/fiber/condition_variable.h"

#include <atomic>
#include <thread>
#include <vector>

#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/base/random.h"
#include "flare/fiber/fiber.h"
#include "flare/fiber/mutex.h"
#include "flare/fiber/this_fiber.h"
#include "flare/fiber/detail/testing.h"

using namespace std::literals;

namespace flare::fiber {

TEST(ConditionVariable, All) {
  testing::RunAsFiber([] {
    for (int k = 0; k != 10; ++k) {
      constexpr auto N = 600;

      std::atomic<std::size_t> run{};
      Mutex lock[N];
      ConditionVariable cv[N];
      bool set[N];
      std::vector<Fiber> prod(N);
      std::vector<Fiber> cons(N);

      for (int i = 0; i != N; ++i) {
        prod[i] = Fiber([&run, i, &cv, &lock, &set] {
          this_fiber::SleepFor(Random(20) * 1ms);
          std::unique_lock lk(lock[i]);
          cv[i].wait(lk, [&] { return set[i]; });
          ++run;
        });
        cons[i] = Fiber([&run, i, &cv, &lock, &set] {
          this_fiber::SleepFor(Random(20) * 1ms);
          std::scoped_lock _(lock[i]);
          set[i] = true;
          cv[i].notify_one();
          ++run;
        });
      }

      for (auto&& e : prod) {
        ASSERT_TRUE(e.joinable());
        e.join();
      }
      for (auto&& e : cons) {
        ASSERT_TRUE(e.joinable());
        e.join();
      }

      ASSERT_EQ(N * 2, run);
    }
  });
}

}  // namespace flare::fiber
