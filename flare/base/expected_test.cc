// Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "flare/base/expected.h"

#include <charconv>
#include <vector>

#include "gtest/gtest.h"

namespace flare {

Expected<int, std::errc> to_int(std::string_view s) {
  int value;
  auto [_, err] = std::from_chars(s.begin(), s.end(), value);
  if (err == std::errc{}) {
    return value;
  }
  return Unexpected(err);
}

Expected<std::string, std::errc> hello_loop(int n) {
  std::string result;
  while (n--) {
    result += "Hello World\n";
  }
  return result;
}

TEST(Expected, Normal) {
  EXPECT_EQ(to_int("42").value(), 42);
  auto foo = to_int("foo");
  EXPECT_FALSE(foo.has_value());
  EXPECT_EQ(foo.error(), std::errc::invalid_argument);
  EXPECT_EQ(to_int("5000000000").error(), std::errc::result_out_of_range);

  Expected<int, int> ex(1);
  EXPECT_TRUE(ex);
  EXPECT_EQ(*ex, 1);

  Expected<std::vector<int>, int> ex2(2);
  EXPECT_TRUE(ex2);
}

TEST(Expected, and_then) {
  auto result =
      to_int("2")
          .and_then(hello_loop)
          .and_then([](const std::string& hello) -> Expected<void, std::errc> {
            std::cout << hello << std::endl;
            return {};
          })
          .and_then([]() -> Expected<std::string, std::errc> { return "a123"; })
          .and_then(to_int)
          .and_then([](int) -> Expected<void, std::errc> {
            EXPECT_FALSE(true);
            return {};
          })
          .and_then([]() -> Expected<void, std::errc> {
            EXPECT_FALSE(true);
            return Unexpected(std::errc::result_out_of_range);
          });
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), std::errc::invalid_argument);
}

TEST(Expected, transform) {
  auto result =
      to_int("42")
          .transform([](int n) { return std::to_string(n); })
          .transform([](std::string_view sv) { EXPECT_STREQ(sv.data(), "42"); })
          .transform([] { return 42; })
          .transform([](int) {});
  EXPECT_TRUE(result.has_value());

  auto result2 = to_int("abs").transform([](int) { EXPECT_FALSE(true); });
  EXPECT_FALSE(result2.has_value());

  auto result3 =
      to_int("5")
          .transform(hello_loop)
          ->transform([](auto sv) { std::cout << sv << std::endl; })
          .and_then([]() -> Expected<void, std::errc> { return {}; })
          .transform([] {})
          .transform([] { return Unexpected(std::errc::result_out_of_range); });
  EXPECT_FALSE(result3.has_value());
  EXPECT_EQ(result3.error(), std::errc::result_out_of_range);
}

TEST(Expected, or_else) {
  auto result = to_int("a123").or_else([](auto ec) -> Expected<int, std::errc> {
    EXPECT_EQ(ec, std::errc::invalid_argument);
    return 123;
  });
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, 123);
  auto result2 = to_int("123")
                     .or_else([](auto ec) -> Expected<int, std::errc> {
                       EXPECT_FALSE(true);
                       return 456;
                     })
                     .or_else([](auto ec) { EXPECT_FALSE(true); });
  EXPECT_TRUE(result2);
  EXPECT_EQ(*result2, 123);
}

TEST(Expected, transform_error) {
  auto result = to_int("2a")
                    .transform_error([](auto ec) {
                      EXPECT_FALSE(true);
                      return "a";
                    })
                    .transform_error([](auto) {
                      EXPECT_FALSE(true);
                      return "hello";
                    })
                    .and_then([](auto value) -> Expected<int, const char*> {
                      EXPECT_EQ(value, 2);
                      return Unexpected("world");
                    })
                    .transform_error([](auto value) {
                      EXPECT_STREQ(value, "world");
                      return 4;
                    });
  EXPECT_FALSE(result);
  EXPECT_EQ(result.error(), 4);
}

}  // namespace flare
