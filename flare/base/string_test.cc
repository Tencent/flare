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

#include "flare/base/string.h"

#include <cmath>

#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/base/random.h"

using namespace std::literals;

namespace flare {

TEST(String, TryParseIntegral) {
  ASSERT_FALSE(TryParse<int>(std::string(123456, 'a')));
  ASSERT_FALSE(TryParse<int>(""));
  ASSERT_FALSE(TryParse<int>("a"));
  ASSERT_FALSE(TryParse<std::int8_t>(std::to_string(INT8_MAX + 1LL)));
  ASSERT_FALSE(TryParse<std::int8_t>(std::to_string(INT8_MIN - 1LL)));
  ASSERT_FALSE(TryParse<std::uint8_t>(std::to_string(UINT8_MAX + 1LL)));
  ASSERT_FALSE(TryParse<std::uint8_t>("-1"));
  ASSERT_FALSE(TryParse<std::int16_t>(std::to_string(INT16_MAX + 1LL)));
  ASSERT_FALSE(TryParse<std::int16_t>(std::to_string(INT16_MIN - 1LL)));
  ASSERT_FALSE(TryParse<std::uint16_t>(std::to_string(UINT16_MAX + 1LL)));
  ASSERT_FALSE(TryParse<std::uint16_t>("-1"));
  ASSERT_FALSE(TryParse<std::int32_t>(std::to_string(INT32_MAX + 1LL)));
  ASSERT_FALSE(TryParse<std::int32_t>(std::to_string(INT32_MIN - 1LL)));
  ASSERT_FALSE(TryParse<std::uint32_t>(std::to_string(UINT32_MAX + 1LL)));
  ASSERT_FALSE(TryParse<std::uint32_t>("-1"));
  ASSERT_FALSE(TryParse<std::int64_t>(std::to_string(INT64_MAX + 1ULL)));
  ASSERT_FALSE(TryParse<std::int64_t>(std::to_string(INT64_MIN) + "0"));
  ASSERT_FALSE(TryParse<std::uint64_t>(std::to_string(UINT64_MAX) + "0"));
  ASSERT_FALSE(TryParse<std::uint64_t>("-1"));
  ASSERT_TRUE(TryParse<int>("0"));
  ASSERT_TRUE(TryParse<std::int8_t>("0"));
  ASSERT_TRUE(TryParse<std::uint8_t>("0"));
  ASSERT_TRUE(TryParse<std::int16_t>("0"));
  ASSERT_TRUE(TryParse<std::uint16_t>("0"));
  ASSERT_TRUE(TryParse<std::int32_t>("0"));
  ASSERT_TRUE(TryParse<std::uint32_t>("0"));
  ASSERT_TRUE(TryParse<std::int64_t>("0"));
  ASSERT_TRUE(TryParse<std::uint64_t>("0"));

  for (int i = -100; i != 100; ++i) {
    auto rc = TryParse<int>(std::to_string(i));
    ASSERT_TRUE(rc);
    ASSERT_EQ(i, *rc);
  }
  for (int i = 0; i != 100000; ++i) {
    auto x = Random<std::int64_t>();
    auto rc = TryParse<std::int64_t>(std::to_string(x));
    ASSERT_TRUE(rc);
    ASSERT_EQ(x, *rc);
  }
  for (int i = 0; i != 100000; ++i) {
    auto x = Random<std::uint64_t>();
    auto rc = TryParse<std::uint64_t>(std::to_string(x));
    ASSERT_TRUE(rc);
    ASSERT_EQ(x, *rc);
  }
}

TEST(String, TryParseFloatingPoint) {
  ASSERT_FALSE(TryParse<float>(""));
  ASSERT_FALSE(TryParse<double>(""));
  ASSERT_FALSE(TryParse<long double>(""));
  ASSERT_FALSE(TryParse<double>("a"));
  ASSERT_FALSE(TryParse<long double>("a"));
  ASSERT_FALSE(TryParse<float>(std::to_string(HUGE_VALF * 10)));
  ASSERT_FALSE(TryParse<float>(std::to_string(-HUGE_VALF * 10)));
  ASSERT_FALSE(TryParse<double>(std::to_string(HUGE_VAL * 10)));
  ASSERT_FALSE(TryParse<double>(std::to_string(-HUGE_VAL * 10)));
  ASSERT_FALSE(TryParse<long double>(std::to_string(HUGE_VALL * 10)));
  ASSERT_FALSE(TryParse<long double>(std::to_string(-HUGE_VALL * 10)));

  for (int i = 0; i != 100000; ++i) {
    auto x = 1.0 * Random() * Random();
    {
      auto rc = TryParse<float>(std::to_string(x));
      ASSERT_TRUE(rc);
      ASSERT_NEAR(x, *rc, fabs(x + *rc) / 2 * 1e-5);
    }
    {
      auto rc = TryParse<double>(std::to_string(x));
      ASSERT_TRUE(rc);
      ASSERT_NEAR(x, *rc, fabs(x + *rc) / 2 * 1e-5);
    }
    {
      auto rc = TryParse<long double>(std::to_string(x));
      ASSERT_TRUE(rc);
      ASSERT_NEAR(x, *rc, fabsl(x + *rc) / 2 * 1e-5);
    }
  }
}

TEST(String, TryParseBool) {
  ASSERT_FALSE(TryParse<bool>(""));
  ASSERT_FALSE(TryParse<bool>(".."));
  ASSERT_FALSE(TryParse<bool>("2"));

  ASSERT_TRUE(TryParse<bool>("1"));
  ASSERT_TRUE(TryParse<bool>("0"));
  ASSERT_TRUE(TryParse<bool>("y"));
  ASSERT_TRUE(TryParse<bool>("n"));
  ASSERT_TRUE(TryParse<bool>("Y"));
  ASSERT_TRUE(TryParse<bool>("N"));
  ASSERT_TRUE(TryParse<bool>("Yes"));
  ASSERT_TRUE(TryParse<bool>("nO"));
  ASSERT_TRUE(TryParse<bool>("TRue"));
  ASSERT_TRUE(TryParse<bool>("faLse"));

  ASSERT_TRUE(*TryParse<bool>("1"));
  ASSERT_FALSE(*TryParse<bool>("0"));
  ASSERT_TRUE(*TryParse<bool>("y"));
  ASSERT_FALSE(*TryParse<bool>("n"));
  ASSERT_TRUE(*TryParse<bool>("Y"));
  ASSERT_FALSE(*TryParse<bool>("N"));
  ASSERT_TRUE(*TryParse<bool>("yeS"));
  ASSERT_FALSE(*TryParse<bool>("No"));
  ASSERT_TRUE(*TryParse<bool>("tRUe"));
  ASSERT_FALSE(*TryParse<bool>("falsE"));
}

TEST(String, StartsWith) {
  ASSERT_TRUE(StartsWith("asdf", "asdf"));
  ASSERT_TRUE(StartsWith("asdf", "asd"));
  ASSERT_TRUE(StartsWith("asdf", "as"));
  ASSERT_TRUE(StartsWith("asdf", "a"));
  ASSERT_TRUE(StartsWith("asdf", ""));
  ASSERT_TRUE(StartsWith("", ""));
  ASSERT_TRUE(!StartsWith("asdf", "b"));
  ASSERT_TRUE(!StartsWith("", "b"));
}

TEST(String, EndsWith) {
  ASSERT_TRUE(EndsWith("asdf", "asdf"));
  ASSERT_TRUE(EndsWith("asdf", "sdf"));
  ASSERT_TRUE(EndsWith("asdf", "df"));
  ASSERT_TRUE(EndsWith("asdf", "f"));
  ASSERT_TRUE(EndsWith("asdf", ""));
  ASSERT_TRUE(EndsWith("", ""));
  ASSERT_TRUE(!EndsWith("asdf", "b"));
  ASSERT_TRUE(!EndsWith("", "b"));
}

TEST(String, Replace) {
  ASSERT_EQ("//////", Replace("////////////", "//", "/"));
  ASSERT_EQ("aabb", Replace("bbbb", "b", "a", 2));
  ASSERT_EQ("/././././", Replace("/.//.//.//./", "//", "/"));
  ASSERT_EQ("/.//./././", Replace("/.///.//.//./", "//", "/"));
  ASSERT_EQ("abbb", Replace("bbbb", "b", "a", 1));
  ASSERT_EQ("//", Replace("//", "/", "/"));
  ASSERT_EQ("", Replace("//", "/", ""));
  ASSERT_EQ("//", Replace("///", "//", "/"));
}

TEST(String, Trim) {
  ASSERT_EQ("", Trim(""));
  ASSERT_EQ("", Trim(" "));
  ASSERT_EQ("", Trim("  "));
  ASSERT_EQ("", Trim("   "));
  ASSERT_EQ("aa", Trim("aa"));
  ASSERT_EQ("aa", Trim(" aa"));
  ASSERT_EQ("aa", Trim("  aa"));
  ASSERT_EQ("aa", Trim("aa "));
  ASSERT_EQ("aa", Trim("aa  "));
  ASSERT_EQ("aa", Trim(" aa "));
  ASSERT_EQ("aa", Trim("  aa "));
  ASSERT_EQ("aa", Trim(" aa  "));
  ASSERT_EQ("aa", Trim("  aa  "));
  ASSERT_EQ("a a", Trim("  a a  "));
}

TEST(String, Split1) {
  auto splited = Split("/a/b/c/d/e/f///g", '/');
  ASSERT_EQ(7, splited.size());
  ASSERT_EQ("a", splited[0]);
  ASSERT_EQ("b", splited[1]);
  ASSERT_EQ("c", splited[2]);
  ASSERT_EQ("d", splited[3]);
  ASSERT_EQ("e", splited[4]);
  ASSERT_EQ("f", splited[5]);
  ASSERT_EQ("g", splited[6]);
}

TEST(String, Split2) {
  auto splited = Split("a///g/", '/');
  ASSERT_EQ(2, splited.size());
  ASSERT_EQ("a", splited[0]);
  ASSERT_EQ("g", splited[1]);
}

TEST(String, Split3) {
  auto splited = Split("/////a/g", '/');
  ASSERT_EQ(2, splited.size());
  ASSERT_EQ("a", splited[0]);
  ASSERT_EQ("g", splited[1]);
}

TEST(String, Split4) {
  auto splited = Split("/////a/g///", '/');
  ASSERT_EQ(2, splited.size());
  ASSERT_EQ("a", splited[0]);
  ASSERT_EQ("g", splited[1]);
}

TEST(String, Split5) {
  auto splited = Split("////a//g//", "//");
  ASSERT_EQ(2, splited.size());
  ASSERT_EQ("a", splited[0]);
  ASSERT_EQ("g", splited[1]);
}

TEST(String, Split6) {
  auto splited = Split("//a//g", "//");
  ASSERT_EQ(2, splited.size());
  ASSERT_EQ("a", splited[0]);
  ASSERT_EQ("g", splited[1]);
}

TEST(String, Split7) {
  auto splited = Split("a//g", "//");
  ASSERT_EQ(2, splited.size());
  ASSERT_EQ("a", splited[0]);
  ASSERT_EQ("g", splited[1]);
}

TEST(String, Split8) {
  auto splited = Split("//", "//");
  ASSERT_EQ(0, splited.size());
}

TEST(String, Split9) {
  auto splited = Split("", '/');
  ASSERT_EQ(0, splited.size());
}

TEST(String, SplitKeepEmpty1) {
  auto splited = Split("///a//g///", '/', true);
  ASSERT_EQ(9, splited.size());
  ASSERT_EQ("", splited[0]);
  ASSERT_EQ("", splited[1]);
  ASSERT_EQ("", splited[2]);
  ASSERT_EQ("a", splited[3]);
  ASSERT_EQ("", splited[4]);
  ASSERT_EQ("g", splited[5]);
  ASSERT_EQ("", splited[6]);
  ASSERT_EQ("", splited[7]);
  ASSERT_EQ("", splited[8]);
}

TEST(String, SplitKeepEmpty2) {
  auto splited = Split("//", "//", true);
  ASSERT_EQ(2, splited.size());
  ASSERT_EQ("", splited[0]);
  ASSERT_EQ("", splited[1]);
}

TEST(String, Join) {
  ASSERT_EQ("a\nbb\nccc", Join({"a"sv, "bb"sv, "ccc"sv}, "\n"));
  ASSERT_EQ("a\n\nbb\nccc", Join({"a"sv, ""sv, "bb"sv, "ccc"sv}, "\n"));
  ASSERT_EQ("a\n\nbb\nccc", Join({"a", "", "bb", "ccc"}, "\n"));
  std::vector<std::string> s = {"a", "", "bb", "ccc"};
  ASSERT_EQ("a\n\nbb\nccc", Join(s, "\n"));
}

TEST(String, IEquals) {
  ASSERT_TRUE(IEquals("abc", "abc"));
  ASSERT_TRUE(IEquals("abc", "aBc"));
  ASSERT_TRUE(IEquals("abc", "ABC"));
  ASSERT_FALSE(IEquals("abc", "ab"));
  ASSERT_FALSE(IEquals("abc", "abcd"));
  ASSERT_FALSE(IEquals("abc", "d"));
}

TEST(String, ToUpper) {
  std::string s = "abCD";
  ToUpper(&s);
  ASSERT_EQ("ABCD", s);
  ASSERT_EQ("ABCD", ToUpper("aBCd"));
}

TEST(String, ToLower) {
  std::string s = "abCD";
  ToLower(&s);
  ASSERT_EQ("abcd", s);
  ASSERT_EQ("abcd", ToLower("aBCd"));
}

}  // namespace flare
