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

#include "flare/net/redis/redis_command.h"

#include "gtest/gtest.h"

#include "flare/base/buffer.h"

using namespace std::literals;

namespace flare {

TEST(RedisCommand, All) {
  RedisCommand cmd1("MSET", "key1", CreateBufferSlow("value1"), "key2",
                    CreateBufferSlow("value2"));

  std::vector<std::string> args{"key1", "value1", "key2", "value2"};
  RedisCommand cmd2("MSET", args);

  EXPECT_EQ(FlattenSlow(cmd1.GetBytes()), FlattenSlow(cmd2.GetBytes()));
  EXPECT_EQ(
      "*5\r\n"
      "$4\r\nMSET\r\n"
      "$4\r\nkey1\r\n"
      "$6\r\nvalue1\r\n"
      "$4\r\nkey2\r\n"
      "$6\r\nvalue2\r\n",
      FlattenSlow(cmd1.GetBytes()));
}

TEST(RedisCommandBuilder, All) {
  RedisCommandBuilder builder;
  builder.Append("MSET");
  builder.Append("key1");
  builder.Append(CreateBufferSlow("value1"));
  builder.Append("key2");
  builder.Append(CreateBufferSlow("value2"));
  auto cmd = builder.DestructiveGet();
  auto str = FlattenSlow(cmd.GetBytes());

  EXPECT_EQ(
      "*5\r\n"
      "$4\r\nMSET\r\n"
      "$4\r\nkey1\r\n"
      "$6\r\nvalue1\r\n"
      "$4\r\nkey2\r\n"
      "$6\r\nvalue2\r\n",
      str);
}

}  // namespace flare
