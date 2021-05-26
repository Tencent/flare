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

#include "flare/net/redis/reader.h"

#include <chrono>
#include <string>

#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/base/buffer.h"

using namespace std::literals;

namespace flare::redis {

RedisObject ParseFromStringMustSucceed(const std::string& str) {
  auto buffer = CreateBufferSlow(str);
  RedisObject obj;
  EXPECT_EQ(1, TryCutRedisObject(&buffer, &obj));
  return obj;
}

// @sa: https://redis.io/topics/protocol

TEST(Reader, String) {
  auto parsed = ParseFromStringMustSucceed("+OK\r\n");
  ASSERT_TRUE(parsed.is<RedisString>());
  EXPECT_EQ("OK", *parsed.as<RedisString>());
}

TEST(Reader, Error1) {
  auto parsed = ParseFromStringMustSucceed("-Error message\r\n");
  ASSERT_TRUE(parsed.is<RedisError>());
}

TEST(Reader, Error2) {
  auto parsed = ParseFromStringMustSucceed(
      "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n");
  ASSERT_TRUE(parsed.is<RedisError>());
  EXPECT_EQ("WRONGTYPE", parsed.as<RedisError>()->category);
  EXPECT_EQ("Operation against a key holding the wrong kind of value",
            parsed.as<RedisError>()->message);
}

TEST(Reader, Integer) {
  auto parsed = ParseFromStringMustSucceed(":1000\r\n");
  ASSERT_TRUE(parsed.is<RedisInteger>());
  EXPECT_EQ(1000, *parsed.as<RedisInteger>());
  parsed = ParseFromStringMustSucceed(":0\r\n");
  ASSERT_TRUE(parsed.is<RedisInteger>());
  EXPECT_EQ(0, *parsed.as<RedisInteger>());
}

TEST(Reader, BulkString1) {
  auto parsed = ParseFromStringMustSucceed("$6\r\nfoobar\r\n");
  ASSERT_TRUE(parsed.is<RedisBytes>());
  EXPECT_EQ("foobar", FlattenSlow(*parsed.as<RedisBytes>()));
}

TEST(Reader, BulkString2) {
  auto parsed = ParseFromStringMustSucceed("$0\r\n\r\n");
  ASSERT_TRUE(parsed.is<RedisBytes>());
  EXPECT_TRUE(parsed.as<RedisBytes>()->Empty());
}

TEST(Reader, BulkString3) {
  auto parsed = ParseFromStringMustSucceed("$-1\r\n");

  // https://redis.io/topics/protocol:
  //
  // > The client library API should not return an empty string, but a nil
  // > object, when the server replies with a Null Bulk String.
  ASSERT_TRUE(parsed.is<RedisNull>());
}

TEST(Reader, Array1) {
  auto parsed = ParseFromStringMustSucceed("*0\r\n");
  ASSERT_TRUE(parsed.is<RedisArray>());
  EXPECT_TRUE(parsed.as<RedisArray>()->empty());
}

TEST(Reader, Array2) {
  auto parsed = ParseFromStringMustSucceed("*2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n");
  ASSERT_TRUE(parsed.is<RedisArray>());

  auto&& elements = *parsed.as<RedisArray>();
  ASSERT_EQ(2, elements.size());
  ASSERT_TRUE(elements[0].is<RedisBytes>());
  ASSERT_TRUE(elements[1].is<RedisBytes>());
  EXPECT_EQ("foo", FlattenSlow(*elements[0].as<RedisBytes>()));
  EXPECT_EQ("bar", FlattenSlow(*elements[1].as<RedisBytes>()));
}

TEST(Reader, Array3) {
  auto parsed = ParseFromStringMustSucceed("*3\r\n:1\r\n:2\r\n:3\r\n");
  ASSERT_TRUE(parsed.is<RedisArray>());

  auto&& elements = *parsed.as<RedisArray>();
  ASSERT_EQ(3, elements.size());
  ASSERT_TRUE(elements[0].is<RedisInteger>());
  ASSERT_TRUE(elements[1].is<RedisInteger>());
  ASSERT_TRUE(elements[2].is<RedisInteger>());
  EXPECT_EQ(1, *elements[0].as<RedisInteger>());
  EXPECT_EQ(2, *elements[1].as<RedisInteger>());
  EXPECT_EQ(3, *elements[2].as<RedisInteger>());
}

TEST(Reader, Array4) {
  auto parsed = ParseFromStringMustSucceed(
      "*5\r\n:1\r\n:2\r\n:3\r\n:4\r\n$6\r\nfoobar\r\n");
  ASSERT_TRUE(parsed.is<RedisArray>());

  auto&& elements = *parsed.as<RedisArray>();
  ASSERT_EQ(5, elements.size());
  ASSERT_TRUE(elements[0].is<RedisInteger>());
  ASSERT_TRUE(elements[1].is<RedisInteger>());
  ASSERT_TRUE(elements[2].is<RedisInteger>());
  ASSERT_TRUE(elements[3].is<RedisInteger>());
  ASSERT_TRUE(elements[4].is<RedisBytes>());
  EXPECT_EQ(1, *elements[0].as<RedisInteger>());
  EXPECT_EQ(2, *elements[1].as<RedisInteger>());
  EXPECT_EQ(3, *elements[2].as<RedisInteger>());
  EXPECT_EQ(4, *elements[3].as<RedisInteger>());
  EXPECT_EQ("foobar", FlattenSlow(*elements[4].as<RedisBytes>()));
}

TEST(Reader, Array5) {
  auto parsed = ParseFromStringMustSucceed("*-1\r\n");

  // https://redis.io/topics/protocol:
  //
  // > A client library API should return a null object and not an empty Array
  // > when Redis replies with a Null Array.
  ASSERT_TRUE(parsed.is<RedisNull>());
}

TEST(Reader, Array6) {
  auto parsed = ParseFromStringMustSucceed(
      "*2\r\n*3\r\n:1\r\n:2\r\n:3\r\n*2\r\n+Foo\r\n-Bar\r\n");
  ASSERT_TRUE(parsed.is<RedisArray>());

  auto&& l1_array = *parsed.as<RedisArray>();
  ASSERT_EQ(2, l1_array.size());
  ASSERT_TRUE(l1_array[0].is<RedisArray>());
  ASSERT_TRUE(l1_array[1].is<RedisArray>());

  auto&& array1 = *l1_array[0].as<RedisArray>();
  auto&& array2 = *l1_array[1].as<RedisArray>();

  ASSERT_EQ(3, array1.size());
  ASSERT_TRUE(array1[0].is<RedisInteger>());
  ASSERT_TRUE(array1[1].is<RedisInteger>());
  ASSERT_TRUE(array1[2].is<RedisInteger>());
  EXPECT_EQ(1, *array1[0].as<RedisInteger>());
  EXPECT_EQ(2, *array1[1].as<RedisInteger>());
  EXPECT_EQ(3, *array1[2].as<RedisInteger>());

  ASSERT_EQ(2, array2.size());
  ASSERT_TRUE(array2[0].is<RedisString>());
  ASSERT_TRUE(array2[1].is<RedisError>());
  EXPECT_EQ("Foo", *array2[0].as<RedisString>());
  EXPECT_EQ("Bar", array2[1].as<RedisError>()->message);
}

TEST(Reader, Array7) {
  auto parsed =
      ParseFromStringMustSucceed("*3\r\n$3\r\nfoo\r\n$-1\r\n$3\r\nbar\r\n");
  ASSERT_TRUE(parsed.is<RedisArray>());

  auto&& elements = *parsed.as<RedisArray>();
  ASSERT_EQ(3, elements.size());
  ASSERT_TRUE(elements[0].is<RedisBytes>());
  ASSERT_TRUE(elements[1].is<RedisNull>());
  ASSERT_TRUE(elements[2].is<RedisBytes>());
  EXPECT_EQ("foo", FlattenSlow(*elements[0].as<RedisBytes>()));
  EXPECT_EQ("bar", FlattenSlow(*elements[2].as<RedisBytes>()));
}

}  // namespace flare::redis
