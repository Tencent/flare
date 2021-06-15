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

#include "flare/base/never_destroyed.h"

#include "gtest/gtest.h"

namespace flare {

struct C {
  C() { ++instances; }
  ~C() { --instances; }
  inline static std::size_t instances{};
};

struct D {
  void foo() {
    [[maybe_unused]] static NeverDestroyedSingleton<D> test_compilation2;
  }
};

NeverDestroyed<int> test_compilation2;

TEST(NeverDestroyed, All) {
  ASSERT_EQ(0, C::instances);
  {
    C c1;
    ASSERT_EQ(1, C::instances);
    [[maybe_unused]] NeverDestroyed<C> c2;
    ASSERT_EQ(2, C::instances);
  }
  // Not 0, as `NeverDestroyed<C>` is not destroyed.
  ASSERT_EQ(1, C::instances);
}

}  // namespace flare
