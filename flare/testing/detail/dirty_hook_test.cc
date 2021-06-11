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

#include "flare/testing/detail/dirty_hook.h"

#include <sstream>
#include <string>

#include "googletest/gtest/gtest.h"

namespace flare::testing::detail {

[[gnu::noinline]] std::string SomeBigFunction(std::string s) {
  std::stringstream ss;
  ss << s << 12345;
  return ss.str();
}

std::string MyBigFunction(std::string s) { return s + " from my big function"; }

TEST(DirtyHook, All) {
  for (int i = 0; i != 100000; ++i) {
    EXPECT_EQ("hello12345", SomeBigFunction("hello"));
    auto handle = InstallHook(reinterpret_cast<void*>(SomeBigFunction),
                              reinterpret_cast<void*>(MyBigFunction));
    EXPECT_EQ("hello from my big function", SomeBigFunction("hello"));
    UninstallHook(handle);
    EXPECT_EQ("hello12345", SomeBigFunction("hello"));
  }
}

}  // namespace flare::testing::detail
