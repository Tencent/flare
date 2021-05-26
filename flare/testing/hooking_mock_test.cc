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

#include "flare/testing/hooking_mock.h"

#include <string>
#include <vector>

#include "thirdparty/googletest/gtest/gtest.h"

namespace flare::testing {

std::string xx;

[[gnu::noinline]] void FancyNonVirtualMethod(std::string x) { xx = x; }

struct FirstStructure {
  [[gnu::noinline]] void AnotherFancyNonVirtualMethod(std::string x) {
    xx = x + "a";
  }
};

struct SecondStructure {
  [[gnu::noinline]] void YetAnotherFancyNonVirtualMethod(std::string x) const {
    xx = x + "b";
  }
};

TEST(HookingMock, NotEnabled) {
  for (int i = 0; i != 12345; ++i) {
    FancyNonVirtualMethod(std::to_string(i));
    ASSERT_EQ(std::to_string(i), xx);
  }
}

TEST(HookingMock, NormalAndMultipleExpect) {
  std::vector<std::string> v;

  FLARE_EXPECT_HOOKED_CALL(FancyNonVirtualMethod, "a")
      .WillRepeatedly([&](std::string x) { v.push_back("b"); });
  FancyNonVirtualMethod("a");
  ASSERT_EQ(1, v.size());
  EXPECT_EQ("b", v[0]);
  v.clear();

  FLARE_EXPECT_HOOKED_CALL(FancyNonVirtualMethod, ::testing::_)
      .WillRepeatedly([&](std::string x) { v.push_back(x); });

  ASSERT_TRUE(v.empty());
  for (int i = 0; i != 12345; ++i) {
    FancyNonVirtualMethod(std::to_string(i));
    ASSERT_EQ(std::to_string(i), v.back());
  }
}

TEST(HookingMock, Member) {
  FirstStructure fs;
  std::vector<std::string> v;
  FLARE_EXPECT_HOOKED_CALL(&FirstStructure::AnotherFancyNonVirtualMethod, &fs,
                           ::testing::_)
      .WillRepeatedly([&](FirstStructure*, std::string x) { v.push_back(x); });
  ASSERT_TRUE(v.empty());
  for (int i = 0; i != 12345; ++i) {
    fs.AnotherFancyNonVirtualMethod(std::to_string(i));
    ASSERT_EQ(std::to_string(i), v.back());
  }
}

TEST(HookingMock, ConstMember) {
  SecondStructure ss;
  std::vector<std::string> v;
  FLARE_EXPECT_HOOKED_CALL(&SecondStructure::YetAnotherFancyNonVirtualMethod,
                           &ss, ::testing::_)
      .WillRepeatedly(
          [&](const SecondStructure*, std::string x) { v.push_back(x); });
  ASSERT_TRUE(v.empty());
  for (int i = 0; i != 12345; ++i) {
    ss.YetAnotherFancyNonVirtualMethod(std::to_string(i));
    ASSERT_EQ(std::to_string(i), v.back());
  }
}

}  // namespace flare::testing
