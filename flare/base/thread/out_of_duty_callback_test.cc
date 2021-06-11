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

#include "flare/base/thread/out_of_duty_callback.h"

#include <chrono>
#include <thread>

#include "googletest/gtest/gtest.h"

using namespace std::literals;

namespace flare {

TEST(OutOfDutyCallback, All) {
  int x = 0;

  auto id = SetThreadOutOfDutyCallback([&] { ++x; }, 1ms);

  std::this_thread::sleep_for(100ms);
  std::thread([] { NotifyThreadOutOfDutyCallbacks(); }).join();
  EXPECT_EQ(1, x);  // Every Thread Matters.

  NotifyThreadOutOfDutyCallbacks();
  EXPECT_EQ(2, x);  // Callback fired.
  NotifyThreadOutOfDutyCallbacks();
  EXPECT_EQ(2, x);  // Rate-throttled.

  std::this_thread::sleep_for(100ms);
  NotifyThreadOutOfDutyCallbacks();
  EXPECT_EQ(3, x);  // Fired again.

  DeleteThreadOutOfDutyCallback(id);
  std::this_thread::sleep_for(100ms);
  NotifyThreadOutOfDutyCallbacks();
  EXPECT_EQ(3, x);  // Our callback has been removed, nothing changed.
}

}  // namespace flare
