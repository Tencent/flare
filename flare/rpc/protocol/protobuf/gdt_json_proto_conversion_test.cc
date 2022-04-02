// Copyright (C) 2022 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "flare/rpc/protocol/protobuf/gdt_json_proto_conversion.h"

#include "gmock/gmock.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"
#include "jsoncpp/json.h"

#include "flare/testing/message.pb.h"

namespace flare::protobuf {

TEST(GdtJsonProtoConversion, Basic) {
  testing::ComplexMessage msg;

  msg.set_integer(5);
  msg.set_boolean(true);
  msg.set_str("abc");
  msg.add_numbers(1);
  msg.add_numbers(2);
  msg.add_numbers(3);
  msg.mutable_one()->set_str("one");
  msg.add_strs("a");
  msg.add_strs("b");
  msg.add_strs("c");

  std::string json;
  ASSERT_TRUE(ProtoMessageToJson(msg, &json, nullptr));

  Json::Reader reader;
  Json::Value value;
  ASSERT_TRUE(reader.parse(json, value));
  EXPECT_EQ(5, value["integer"].asInt());
  EXPECT_EQ(true, value["boolean"].asBool());
  EXPECT_EQ("abc", value["str"].asString());
  EXPECT_EQ("one", value["one"]["str"].asString());
  EXPECT_EQ(3, value["numbers"].size());
  EXPECT_EQ(1, value["numbers"][0].asInt());
  EXPECT_EQ(2, value["numbers"][1].asInt());
  EXPECT_EQ(3, value["numbers"][2].asInt());
  EXPECT_EQ(3, value["strs"].size());
  EXPECT_EQ("a", value["strs"][0].asString());
  EXPECT_EQ("b", value["strs"][1].asString());
  EXPECT_EQ("c", value["strs"][2].asString());

  testing::ComplexMessage msg2;
  ASSERT_TRUE(JsonToProtoMessage(json, &msg2, nullptr));
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(msg, msg2));
}

TEST(GdtJsonProtoConversion, StringToIntegerImplicitConversion) {
  // Special case: GDT JSON converter supports implicit conversion from string
  // to integral types.
  testing::ComplexMessage msg;
  auto json = R"({"numbers":["1",2,"3"],"integer":"7"})";
  ASSERT_TRUE(JsonToProtoMessage(json, &msg, nullptr));
  EXPECT_THAT(msg.numbers(), ::testing::ElementsAre(1, 2, 3));
  EXPECT_EQ(7, msg.integer());
}

}  // namespace flare::protobuf
