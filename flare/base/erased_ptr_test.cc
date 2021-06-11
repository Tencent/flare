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

#include "flare/base/erased_ptr.h"

#include "googletest/gtest/gtest.h"

namespace flare {

struct C {
  C() { ++instances; }
  ~C() { --instances; }
  inline static int instances = 0;
};

TEST(ErasedPtr, All) {
  ASSERT_EQ(0, C::instances);
  {
    ErasedPtr ptr(new C{});
    ASSERT_EQ(1, C::instances);
    auto deleter = ptr.GetDeleter();
    auto p = ptr.Leak();
    ASSERT_EQ(1, C::instances);
    deleter(p);
    ASSERT_EQ(0, C::instances);
  }
  ASSERT_EQ(0, C::instances);
  {
    ErasedPtr ptr(new C{});
    ASSERT_EQ(1, C::instances);
  }
  ASSERT_EQ(0, C::instances);
}

}  // namespace flare
