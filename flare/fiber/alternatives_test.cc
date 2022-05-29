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

#include "flare/fiber/alternatives.h"

#include <errno.h>

#include <iostream>
#include <thread>

#include "gtest/gtest.h"

#include "flare/fiber/detail/testing.h"
#include "flare/fiber/this_fiber.h"

namespace flare::fiber {

TEST(LastError, All) {
  fiber::testing::RunAsFiber([&] {
    auto was = GetCurrentThreadId();
    SetLastError(9999);
    EXPECT_EQ(9999, errno);
    EXPECT_EQ(9999, GetLastError());
    while (was == GetCurrentThreadId()) {
      this_fiber::Yield();
    }
    EXPECT_NE(9999, GetLastError());
    std::cout << "errno = " << errno << std::endl;  // May or may not be 9999.
  });
}

TEST(ThreadId, All) {
  fiber::testing::RunAsFiber(
      [&] { EXPECT_EQ(GetCurrentThreadId(), std::this_thread::id()); });
}

}  // namespace flare::fiber
