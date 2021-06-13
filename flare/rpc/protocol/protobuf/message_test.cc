// Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "flare/rpc/protocol/protobuf/message.h"

#include <utility>

#include "gtest/gtest.h"

#include "flare/testing/main.h"

namespace flare::protobuf {

static const std::pair<MessageFactory::Type, rpc::Status> kExpected[] = {
    {MessageFactory::Type::Overloaded, rpc::STATUS_OVERLOADED},
    {MessageFactory::Type::CircuitBroken, rpc::STATUS_OVERLOADED}};

TEST(ErrorMessageFactory, CreateNormal) {
  for (auto&& [t, s] : kExpected) {
    auto msg = error_message_factory.Create(t, 123, false);
    auto pmsg = dyn_cast<ProtoMessage>(msg.get());
    ASSERT_TRUE(!!pmsg);
    ASSERT_FALSE(!!std::get<1>(pmsg->msg_or_buffer));
    ASSERT_EQ(rpc::METHOD_TYPE_SINGLE, pmsg->meta->method_type());
    ASSERT_EQ(123, pmsg->meta->correlation_id());
    ASSERT_EQ(s, pmsg->meta->response_meta().status());
  }
}

TEST(ErrorMessageFactory, CreateStream) {
  for (auto&& [t, s] : kExpected) {
    auto msg = error_message_factory.Create(t, 123, true);
    auto pmsg = dyn_cast<ProtoMessage>(msg.get());
    ASSERT_TRUE(!!pmsg);
    ASSERT_FALSE(!!std::get<1>(pmsg->msg_or_buffer));
    ASSERT_EQ(rpc::METHOD_TYPE_STREAM, pmsg->meta->method_type());
    ASSERT_EQ(
        rpc::MESSAGE_FLAGS_START_OF_STREAM | rpc::MESSAGE_FLAGS_END_OF_STREAM,
        pmsg->meta->flags());
    ASSERT_EQ(123, pmsg->meta->correlation_id());
    ASSERT_EQ(s, pmsg->meta->response_meta().status());
  }
}

}  // namespace flare::protobuf

FLARE_TEST_MAIN
