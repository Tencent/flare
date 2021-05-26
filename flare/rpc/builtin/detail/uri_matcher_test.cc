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

#include "flare/rpc/builtin/detail/uri_matcher.h"

#include "thirdparty/googletest/gtest/gtest.h"

namespace flare::detail {

TEST(UriMatcher, All) {
  UriMatcher matcher0;
  UriMatcher matcher1("/some_prefix");
  UriMatcher matcher2(std::regex("\\abc.*efg"));
  UriMatcher matcher3([](auto&& s) { return s == "1"; });

  EXPECT_TRUE(matcher0(""));
  EXPECT_TRUE(matcher0("a"));
  EXPECT_TRUE(matcher0("abc//"));
  EXPECT_TRUE(matcher0("/some_prefix"));

  EXPECT_TRUE(matcher1("/some_prefix"));
  EXPECT_TRUE(matcher1("/some_prefix1"));
  EXPECT_TRUE(matcher1("/some_prefix/1"));
  EXPECT_FALSE(matcher1(""));
  EXPECT_FALSE(matcher1("/some_prefi"));

  EXPECT_TRUE(matcher2("abcdefg"));
  EXPECT_TRUE(matcher2("abcefg"));
  EXPECT_TRUE(matcher2("abc12345efg"));
  EXPECT_FALSE(matcher2("abcfg"));
  EXPECT_FALSE(matcher2(""));

  EXPECT_TRUE(matcher3("1"));
  EXPECT_FALSE(matcher3(""));
  EXPECT_FALSE(matcher3("12"));
}

}  // namespace flare::detail
