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

#include "flare/rpc/internal/sampler.h"

#include <chrono>

#include "googletest/gtest/gtest.h"

#include "flare/base/chrono.h"

using namespace std::literals;

namespace flare::rpc::detail {

TEST(LargeIntervalSampler, All) {
  int count = 0;

  LargeIntervalSampler sampler(100ms);
  auto stop = ReadCoarseSteadyClock() + 1s;
  while (ReadCoarseSteadyClock() <= stop) {
    count += sampler.Sample();
  }

  EXPECT_NEAR(count, 10, 2);
}

TEST(EveryNSampler, All) {
  int count = 0;

  EveryNSampler sampler(100);
  for (int i = 0; i != 1000; ++i) {
    count += sampler.Sample();
  }

  ASSERT_EQ(10, count);
}

}  // namespace flare::rpc::detail
