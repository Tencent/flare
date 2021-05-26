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

#include "flare/base/enum.h"

#include <cstddef>

#include "thirdparty/googletest/gtest/gtest.h"

namespace flare {

TEST(Enum, UnderlyingValue) {
  std::byte b{10};
  ASSERT_EQ(10, underlying_value(b));
}

TEST(Enum, OperatorOr) {
  std::byte a{1}, b{2};
  ASSERT_EQ(3, underlying_value(a | b));
  a |= b;
  ASSERT_EQ(3, underlying_value(a));
}

TEST(Enum, OperatorAnd) {
  std::byte a{3}, b{2};
  ASSERT_EQ(2, underlying_value(a & b));
  a &= b;
  ASSERT_EQ(2, underlying_value(a));
}

TEST(Enum, OperatorXor) {
  std::byte a{2}, b{2};
  ASSERT_EQ(0, underlying_value(a ^ b));
  a ^= b;
  ASSERT_EQ(0, underlying_value(a));
}

}  // namespace flare
