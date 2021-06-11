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

#include "flare/base/type_index.h"

#include <string>

#include "googletest/gtest/gtest.h"

namespace flare {

TEST(TypeIndex, Compare) {
  constexpr TypeIndex empty1, empty2;

  ASSERT_EQ(empty1, empty2);

  // Statically initializable.
  constexpr auto str_type = GetTypeIndex<std::string>();
  constexpr auto int_type = GetTypeIndex<int>();

  // `operator !=` is not implemented, we can't use `ASSERT_NE` here.
  ASSERT_FALSE(empty1 == str_type);
  ASSERT_FALSE(empty1 == int_type);
  ASSERT_FALSE(str_type == int_type);

  if (str_type < int_type) {
    ASSERT_FALSE(int_type < str_type);
  } else {
    ASSERT_FALSE(str_type < int_type);
  }
}

TEST(TypeIndex, TypeIndexOfRuntime) {
  constexpr auto str_type = GetTypeIndex<std::string>();
  ASSERT_EQ(std::type_index(typeid(std::string)),
            str_type.GetRuntimeTypeIndex());
}

}  // namespace flare
