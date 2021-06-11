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

#include "flare/net/redis/redis_protocol.h"

#include <chrono>
#include <memory>

#include "googletest/gtest/gtest.h"

#include "flare/base/down_cast.h"
#include "flare/net/redis/message.h"

using namespace std::literals;

namespace flare::redis {

class RedisProtocolTest : public ::testing::Test {
 public:
  void SetUp() override {
    request_.command = &command_;
    protocol_ = std::make_unique<RedisProtocol>();
    buffer_ = std::make_unique<NoncontiguousBuffer>();
  }

 protected:
  RedisObject ParseResponse(const std::string& str) {
    NoncontiguousBuffer buffer = CreateBufferSlow(str);
    EXPECT_EQ(StreamProtocol::MessageCutStatus::Cut,
              protocol_->TryCutMessage(&buffer, &cut_));
    return down_cast<RedisResponse>(*cut_)->object;
  }

  RedisCommand command_{"GET", "1234"};
  RedisRequest request_;
  std::unique_ptr<RedisProtocol> protocol_;
  std::unique_ptr<NoncontiguousBuffer> buffer_;
  std::unique_ptr<Message> cut_;
};

TEST_F(RedisProtocolTest, NoCredential) {
  protocol_->WriteMessage(request_, buffer_.get(), nullptr);
  EXPECT_EQ("*2\r\n$3\r\nGET\r\n$4\r\n1234\r\n", FlattenSlow(*buffer_));
  EXPECT_EQ("Basic String",
            *ParseResponse("+Basic String\r\n").as<RedisString>());
  EXPECT_EQ("Basic String",
            *ParseResponse("+Basic String\r\n").as<RedisString>());
  EXPECT_EQ("Basic String",
            *ParseResponse("+Basic String\r\n").as<RedisString>());
}

TEST_F(RedisProtocolTest, BasicCredential) {
  protocol_->SetCredential("mypass");
  protocol_->WriteMessage(request_, buffer_.get(), nullptr);
  EXPECT_EQ(
      "*2\r\n"
      "$4\r\nAUTH\r\n$6\r\nmypass\r\n"
      "*2\r\n"
      "$3\r\nGET\r\n$4\r\n1234\r\n",
      FlattenSlow(*buffer_));
  EXPECT_EQ("Basic String", *ParseResponse("+OK\r\n"  // To `AUTH`
                                           "+Basic String\r\n")
                                 .as<RedisString>());
  EXPECT_EQ("Basic String",
            *ParseResponse("+Basic String\r\n").as<RedisString>());
  EXPECT_EQ("Basic String",
            *ParseResponse("+Basic String\r\n").as<RedisString>());
}

TEST_F(RedisProtocolTest, ExtendedCredential) {
  protocol_->SetCredential("myuser", "mypass1");
  protocol_->WriteMessage(request_, buffer_.get(), nullptr);
  EXPECT_EQ(
      "*3\r\n"
      "$4\r\nAUTH\r\n$6\r\nmyuser\r\n$7\r\nmypass1\r\n"
      "*2\r\n"
      "$3\r\nGET\r\n$4\r\n1234\r\n",
      FlattenSlow(*buffer_));
  EXPECT_EQ("Basic String", *ParseResponse("+OK\r\n"  // To `AUTH`
                                           "+Basic String\r\n")
                                 .as<RedisString>());
  EXPECT_EQ("Basic String",
            *ParseResponse("+Basic String\r\n").as<RedisString>());
  EXPECT_EQ("Basic String",
            *ParseResponse("+Basic String\r\n").as<RedisString>());
}

TEST_F(RedisProtocolTest, CredentialError) {
  protocol_->SetCredential("mypass");
  EXPECT_EQ("NOAUTH",
            ParseResponse("-ERR invalid password\r\n"  // To `AUTH`
                          "-NOAUTH Authentication required.\r\n")
                .as<RedisError>()
                ->category);
  EXPECT_EQ("NOAUTH", ParseResponse("-NOAUTH Authentication required.\r\n")
                          .as<RedisError>()
                          ->category);
  EXPECT_EQ("NOAUTH", ParseResponse("-NOAUTH Authentication required.\r\n")
                          .as<RedisError>()
                          ->category);
}

}  // namespace flare::redis
