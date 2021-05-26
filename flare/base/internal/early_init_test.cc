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

#include "flare/base/internal/early_init.h"

#include "thirdparty/googletest/gtest/gtest.h"

namespace flare::internal {

struct A {
  A() { ++instances; }
  ~A() { --instances; }
  inline static int instances = 0;
};

TEST(EarlyInit, All) {
  // Instantiated even before first call to `EarlyInit`.
  ASSERT_EQ(3, A::instances);

  EarlyInitConstant<A>();
  ASSERT_EQ(3, A::instances);
  EarlyInitConstant<A>();
  ASSERT_EQ(3, A::instances);
  EarlyInitConstant<A, int>();
  ASSERT_EQ(3, A::instances);
  EarlyInitConstant<A, int>();
  ASSERT_EQ(3, A::instances);
  EarlyInitConstant<A, struct X>();
  ASSERT_EQ(3, A::instances);
}

}  // namespace flare::internal
