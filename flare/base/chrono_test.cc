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

#include "flare/base/chrono.h"

#include <chrono>
#include <thread>

#include "gtest/gtest.h"

using namespace std::literals;

namespace flare {

TEST(SystemClock, Compare) {
  auto diff = (ReadSystemClock() - std::chrono::system_clock::now()) / 1ms;
  ASSERT_NEAR(diff, 0, 2);
}

TEST(SteadyClock, Compare) {
  auto diff = (ReadSteadyClock() - std::chrono::steady_clock::now()) / 1ms;
  ASSERT_NEAR(diff, 0, 2);
}

TEST(CoarseSystemClock, Compare) {
  auto diff =
      (ReadCoarseSystemClock() - std::chrono::system_clock::now()) / 1ms;
  ASSERT_NEAR(diff, 0, 10);
}

TEST(CoarseSteadyClock, Compare) {
  auto diff =
      (ReadCoarseSteadyClock() - std::chrono::steady_clock::now()) / 1ms;
  ASSERT_NEAR(diff, 0, 10);
}

}  // namespace flare
