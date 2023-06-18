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

// clang-format off
#include "flare/base/object_pool.h"
// clang-format on

#include <thread>
#include <vector>

#include "gtest/gtest.h"

using namespace std::literals;

namespace flare {

int alive = 0;

struct C {
  C() { ++alive; }
  ~C() { --alive; }
};

template <>
struct PoolTraits<C> {
  static constexpr auto kType = PoolType::Disabled;
};

namespace object_pool {

TEST(DisabledPool, All) {
  std::vector<PooledPtr<C>> ptrs;
  for (int i = 0; i != 1000; ++i) {
    ptrs.push_back(Get<C>());
  }
  ASSERT_EQ(1000, alive);
  ptrs.clear();
  ASSERT_EQ(0, alive);
  Get<C>();
  ASSERT_EQ(0, alive);
}

}  // namespace object_pool
}  // namespace flare
