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

#include "flare/base/internal/time_keeper.h"

#include <vector>

#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/base/chrono.h"
#include "flare/testing/main.h"

using namespace std::literals;

namespace flare::internal {

TEST(TimeKeeper, FastTimer) {
  int x = 0;
  auto timer_id = TimeKeeper::Instance()->AddTimer(
      ReadSteadyClock(), 10ms, [&](auto) { ++x; }, false);
  std::this_thread::sleep_for(1s);
  TimeKeeper::Instance()->KillTimer(timer_id);
  ASSERT_NEAR(x, 100, 10);
}

TEST(TimeKeeper, SlowTimer) {
  std::vector<std::uint64_t> timers;
  std::atomic<int> x{};
  for (int i = 0; i != 1000; ++i) {
    timers.push_back(TimeKeeper::Instance()->AddTimer(
        ReadSteadyClock(), 10ms, [&](auto) { ++x; }, true));
  }
  std::this_thread::sleep_for(1s);
  for (auto&& e : timers) {
    TimeKeeper::Instance()->KillTimer(e);
  }
  ASSERT_NEAR(x, 1000 * 100, 1000 * 10);
}

}  // namespace flare::internal

FLARE_TEST_MAIN
