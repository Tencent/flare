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

#include "flare/net/http/http_message.h"

#include "thirdparty/googletest/gtest/gtest.h"

namespace flare::http {

TEST(HttpMessage, Header) {
  HttpMessage msg;

  ASSERT_FALSE(msg.headers()->contains("Test"));
  msg.headers()->Append("Test", "V");
  ASSERT_TRUE(msg.headers()->contains("Test"));
  msg.clear();
  ASSERT_FALSE(msg.headers()->contains("Test"));
}

TEST(HttpMessage, Body) {
  HttpMessage msg;

  msg.set_body(CreateBufferSlow("noncontiguous-body"));
  ASSERT_EQ("noncontiguous-body", *msg.body());  // Flattend implicitly.
  ASSERT_EQ("noncontiguous-body", FlattenSlow(*msg.noncontiguous_body()));
  msg.set_body("body");
  ASSERT_FALSE(msg.noncontiguous_body());
  ASSERT_EQ("body", *msg.body());
}

TEST(HttpMessage, BodySize) {
  HttpMessage msg;
  ASSERT_EQ(0, msg.body_size());

  std::string body = "noncontiguous-body";
  msg.set_body(CreateBufferSlow(body));
  ASSERT_EQ(body.size(), msg.body_size());

  body = "body_str";
  msg.set_body(body);
  ASSERT_EQ(body.size(), msg.body_size());
}

}  // namespace flare::http
