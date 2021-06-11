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

#include "flare/testing/redis_mock.h"

#include "googletest/gmock/gmock-matchers.h"
#include "googletest/gtest/gtest.h"

#include "flare/net/redis/redis_channel.h"
#include "flare/net/redis/redis_client.h"
#include "flare/testing/main.h"

using namespace std::literals;

using testing::_;

namespace flare::testing::detail {

TEST(RedisMock, All) {
  RedisChannel channel;
  FLARE_CHECK(channel.Open("mock://whatever-it-wants-to-be."));
  RedisClient client(&channel);

  FLARE_EXPECT_REDIS_COMMAND(RedisCommandEq(RedisCommand("GET", "x")))
      .WillRepeatedly(Return(RedisString("str")));
  FLARE_EXPECT_REDIS_COMMAND(RedisCommandOpEq("SET"))
      .WillRepeatedly(Return(RedisString("str")));
  FLARE_EXPECT_REDIS_COMMAND(RedisCommandUserMatch([](auto&& cmd) {
    return GetRedisCommandOp(cmd) == "SCAN";
  })).WillRepeatedly(Return(RedisString("str")));

  auto result = client.Execute(RedisCommand("GET", "x"), 1s);
  ASSERT_TRUE(result.is<RedisString>());
  EXPECT_EQ("str", *result.as<RedisString>());

  result = client.Execute(RedisCommand("SET", "x", "y"), 1s);
  ASSERT_TRUE(result.is<RedisString>());
  EXPECT_EQ("str", *result.as<RedisString>());

  result = client.Execute(RedisCommand("SCAN", "0"), 1s);
  ASSERT_TRUE(result.is<RedisString>());
  EXPECT_EQ("str", *result.as<RedisString>());

  FLARE_EXPECT_REDIS_COMMAND(_).WillRepeatedly(Return(RedisNull()));

  EXPECT_TRUE(
      client.Execute(RedisCommand("GET", "not existing"), 1s).is<RedisNull>());
}

}  // namespace flare::testing::detail

FLARE_TEST_MAIN
