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

#include "flare/base/internal/lazy_init.h"

#include "thirdparty/googletest/gtest/gtest.h"

namespace flare::internal {

struct A {
  A() { ++instances; }
  ~A() { --instances; }
  inline static int instances = 0;
};

TEST(LazyInit, All) {
  ASSERT_EQ(0, A::instances);
  LazyInit<A>();
  ASSERT_EQ(1, A::instances);
  LazyInit<A>();
  ASSERT_EQ(1, A::instances);
  LazyInit<A>();
  ASSERT_EQ(1, A::instances);
  LazyInit<A>();
  ASSERT_EQ(1, A::instances);
  LazyInit<A>();
  ASSERT_EQ(1, A::instances);
  LazyInit<A, int>();
  ASSERT_EQ(2, A::instances);
  LazyInit<A, int>();
  ASSERT_EQ(2, A::instances);
  LazyInit<A, struct X>();
  ASSERT_EQ(3, A::instances);
}

}  // namespace flare::internal
