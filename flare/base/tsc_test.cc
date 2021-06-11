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

#include "flare/base/tsc.h"

#include <chrono>
#include <thread>

#include "googletest/gtest/gtest.h"

#include "flare/base/chrono.h"
#include "flare/base/logging.h"

using namespace std::literals;

namespace flare {

TEST(Tsc, ReadTsc) {
  auto tsc1 = ReadTsc();
  auto tsc2 = ReadTsc();
  ASSERT_NEAR(tsc1, tsc2, 2'000);  // 1us on 2GHz.
  FLARE_LOG_INFO(
      "Frequency detected: {} MHz.",
      1s * tsc::detail::kUnit / tsc::detail::kNanosecondsPerUnit / 1'000'000.0);
}

TEST(Tsc, TscElapsed) {
  ASSERT_EQ(0, TscElapsed(10, 9));  // TSC goes backwards.
  ASSERT_EQ(0, TscElapsed(10, 10));
  ASSERT_EQ(1, TscElapsed(10, 11));
}

TEST(Tsc, DurationFromTsc) {
  auto tsc1 = ReadTsc();
  auto ts1 = ReadSteadyClock();
  std::this_thread::sleep_for(1s);
  auto duration = DurationFromTsc(tsc1, ReadTsc());
  auto duration2 = ReadSteadyClock() - ts1;
  FLARE_LOG_INFO("Got {}us, {}us.", duration / 1us, duration2 / 1us);
  ASSERT_NEAR(duration2 / 1ms, duration / 1ms, 1);
}

TEST(Tsc, TimestampFromTsc) {
  // Initialize TLS first. Otherwise we'll experience ~5us delay in
  // `TimestampFromTsc`, which fails the UT.
  TimestampFromTsc(ReadTsc());

  auto diff = ReadSteadyClock() - TimestampFromTsc(ReadTsc());
  FLARE_LOG_INFO("Diff {} us.", diff / 1us);
  ASSERT_NEAR(1, diff / 1us, 1);
}

}  // namespace flare
