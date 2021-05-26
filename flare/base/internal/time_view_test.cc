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

#include "flare/base/internal/time_view.h"

#include "thirdparty/googletest/gtest/gtest.h"

using namespace std::literals;

namespace flare::internal {

template <class Expecting>
auto GetTime(TimeView<Expecting> view) {
  return view.Get();
}

#define TIME_NEAR(x, y, diff) ASSERT_NEAR(((x) - (y)) / 1ns, 0, (diff) / 1ns)

TEST(TimeView, TimePoint) {
  TIME_NEAR(GetTime<std::chrono::steady_clock::time_point>(
                std::chrono::system_clock::now() + 1s),
            ReadSteadyClock() + 1s, 100us);
  TIME_NEAR(GetTime<std::chrono::steady_clock::time_point>(
                std::chrono::steady_clock::now() + 1s),
            ReadSteadyClock() + 1s, 100us);
  TIME_NEAR(
      // Do NOT remove the plus sign here.
      GetTime<std::chrono::steady_clock::time_point>(+1s),
      ReadSteadyClock() + 1s, 100us);
  TIME_NEAR(
      GetTime<std::chrono::steady_clock::time_point>(ReadSteadyClock() + 1s),
      ReadSteadyClock() + 1s, 100us);
  TIME_NEAR(
      GetTime<std::chrono::steady_clock::time_point>(ReadSystemClock() + 1s),
      ReadSteadyClock() + 1s, 100us);
}

TEST(TimeView, Duration) {
  TIME_NEAR(
      +1s,
      GetTime<std::chrono::nanoseconds>(std::chrono::system_clock::now() + 1s),
      100us);
  TIME_NEAR(
      +1s,
      GetTime<std::chrono::nanoseconds>(std::chrono::steady_clock::now() + 1s),
      100us);
  TIME_NEAR(+1s, GetTime<std::chrono::nanoseconds>(+1s), 100us);
  TIME_NEAR(+1s, GetTime<std::chrono::nanoseconds>(ReadSteadyClock() + 1s),
            100us);
  TIME_NEAR(+1s, GetTime<std::chrono::nanoseconds>(ReadSystemClock() + 1s),
            100us);
}

}  // namespace flare::internal
