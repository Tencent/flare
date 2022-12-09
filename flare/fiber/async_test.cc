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

#include "flare/fiber/async.h"

#include "gtest/gtest.h"

#include "flare/base/future.h"
#include "flare/fiber/alternatives.h"
#include "flare/fiber/detail/testing.h"
#include "flare/fiber/future.h"
#include "flare/fiber/this_fiber.h"

using namespace std::literals;

namespace flare::fiber {

TEST(Async, Execute) {
  testing::RunAsFiber([] {
    for (int i = 0; i != 10000; ++i) {
      int rc = 0;
      auto tid = GetCurrentThreadId();
      auto ff = Async(Launch::Dispatch, [&] {
        rc = 1;
        ASSERT_EQ(tid, GetCurrentThreadId());
      });
      fiber::BlockingGet(&ff);
      ASSERT_EQ(1, rc);
      Future<int> f = Async([&] {
        // Which thread is running this fiber is unknown. No assertion here.
        return 5;
      });
      this_fiber::Yield();
      ASSERT_EQ(5, fiber::BlockingGet(&f));
    }
  });
}

}  // namespace flare::fiber
