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

#include "flare/net/http/http_headers.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace flare {

TEST(HttpHeaders, All) {
  HttpHeaders headers;
  EXPECT_EQ("", headers.ToString());

  headers.Append("Hello", "World");
  EXPECT_EQ("Hello: World\r\n", headers.ToString());

  headers.Append("Great", "Mall");
  EXPECT_EQ("Hello: World\r\nGreat: Mall\r\n", headers.ToString());

  headers.Append({{"X-My-Header-1", "Value-1"}, {"X-My-Header-2", "Value-2"}});
  EXPECT_EQ(
      "Hello: World\r\n"
      "Great: Mall\r\n"
      "X-My-Header-1: Value-1\r\n"
      "X-My-Header-2: Value-2\r\n",
      headers.ToString());

  ASSERT_EQ("World", *headers.TryGet("Hello"));
  ASSERT_EQ("World", *headers.TryGet("hello"));  // Lower case.
  ASSERT_FALSE(headers.TryGet("Hi"));

  ASSERT_EQ(0, headers.TryGetMultiple("404").size());
  headers.Append("Hello", "World2");
  ASSERT_THAT(headers.TryGetMultiple("Hello"),
              testing::ElementsAre("World", "World2"));
  // Lower case.
  ASSERT_THAT(headers.TryGetMultiple("hello"),
              testing::ElementsAre("World", "World2"));
}

TEST(HttpHeaders, Set) {
  HttpHeaders headers;
  ASSERT_FALSE(headers.TryGet("Hi"));
  headers.Set("Hi", "Abc");
  ASSERT_EQ("Abc", *headers.TryGet("Hi"));
  headers.Set("Hi", "Xyz");
  ASSERT_EQ("Xyz", *headers.TryGet("Hi"));
}

TEST(HttpHeaders, Remove) {
  HttpHeaders headers;
  headers.Append("Hi", "Abc");
  headers.Append("Hello", "World");
  headers.Append("hi", "Def");
  headers.Append("Hi", "Xyz");
  ASSERT_EQ("Abc", *headers.TryGet("Hi"));
  ASSERT_TRUE(headers.Remove("Hi"));
  ASSERT_FALSE(headers.Remove("Hi"));
  ASSERT_FALSE(headers.TryGet("hi"));
  ASSERT_EQ("World", headers.TryGet("Hello"));
}

}  // namespace flare
