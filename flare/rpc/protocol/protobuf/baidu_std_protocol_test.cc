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

#include "flare/rpc/protocol/protobuf/baidu_std_protocol.h"

#include "thirdparty/googletest/gtest/gtest.h"
#include "thirdparty/protobuf/util/message_differencer.h"

#include "flare/init/on_init.h"
#include "flare/rpc/protocol/protobuf/call_context.h"
#include "flare/rpc/protocol/protobuf/message.h"
#include "flare/rpc/protocol/protobuf/service_method_locator.h"
#include "flare/testing/echo_service.flare.pb.h"
#include "flare/testing/main.h"

namespace flare::protobuf {

auto service_desc = testing::EchoService::descriptor();

// We use a lower priority here since method provider (i.e., listeners to
// `AddServiceForXxx`) are registered with priority 0.
FLARE_ON_INIT(10, [] {
  ServiceMethodLocator::Instance()->AddService(service_desc);
});

TEST(BaiduStdProtocol, ClientToServer) {
  auto src = object_pool::Get<rpc::RpcMeta>();
  src->set_correlation_id(1);
  src->set_method_type(rpc::METHOD_TYPE_SINGLE);
  src->mutable_request_meta()->set_method_name(
      "flare.testing.EchoService.Echo");
  src->mutable_request_meta()->set_acceptable_compression_algorithms(
      1 << rpc::COMPRESSION_ALGORITHM_NONE |
      1 << rpc::COMPRESSION_ALGORITHM_GZIP |
      1 << rpc::COMPRESSION_ALGORITHM_SNAPPY);
  auto src_cp = *src;
  testing::EchoRequest payload;
  payload.set_body("asdf");

  BaiduStdProtocol client_prot(false), server_prot(true);
  ProtoMessage msg(std::move(src), MaybeOwning(non_owning, &payload));
  NoncontiguousBuffer buffer;
  ProactiveCallContext pcc;
  pcc.method = service_desc->FindMethodByName("Echo");
  client_prot.WriteMessage(msg, &buffer, &pcc);

  ASSERT_TRUE(
      google::protobuf::util::MessageDifferencer::Equals(*msg.meta, src_cp));
  ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      *std::get<1>(msg.msg_or_buffer), payload));

  std::unique_ptr<Message> parsed;
  PassiveCallContext passive_ctx;
  ASSERT_TRUE(server_prot.TryCutMessage(&buffer, &parsed) ==
              StreamProtocol::MessageCutStatus::Cut);
  ASSERT_TRUE(server_prot.TryParse(&parsed, &passive_ctx));
  ASSERT_EQ(0, buffer.ByteSize());

  // Same as the original one.
  auto parsed_casted = cast<ProtoMessage>(parsed.get());
  ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      *msg.meta, *parsed_casted->meta));
  ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      *std::get<1>(msg.msg_or_buffer),
      *std::get<1>(parsed_casted->msg_or_buffer)));
}

TEST(BaiduStdProtocol, ServerToClient) {
  auto src = object_pool::Get<rpc::RpcMeta>();
  src->set_correlation_id(1);
  src->set_method_type(rpc::METHOD_TYPE_SINGLE);
  src->mutable_response_meta()->set_status(rpc::STATUS_OVERLOADED);
  auto src_cp = *src;
  testing::EchoResponse payload;
  payload.set_body("abcd");

  BaiduStdProtocol server_prot(true), client_prot(false);
  ProtoMessage msg(std::move(src), MaybeOwning(non_owning, &payload));
  NoncontiguousBuffer buffer;
  PassiveCallContext passive_ctx;
  server_prot.WriteMessage(msg, &buffer, &passive_ctx);

  ASSERT_TRUE(
      google::protobuf::util::MessageDifferencer::Equals(*msg.meta, src_cp));
  ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      *std::get<1>(msg.msg_or_buffer), payload));

  testing::EchoResponse unpack_to;
  std::unique_ptr<Message> parsed;
  ProactiveCallContext pcc;
  pcc.accept_response_in_bytes = false;
  pcc.expecting_stream = false;
  pcc.response_ptr = &unpack_to;
  ASSERT_TRUE(client_prot.TryCutMessage(&buffer, &parsed) ==
              StreamProtocol::MessageCutStatus::Cut);
  ASSERT_TRUE(client_prot.TryParse(&parsed, &pcc));
  ASSERT_EQ(0, buffer.ByteSize());

  // Same as the original one.
  auto parsed_casted = cast<ProtoMessage>(parsed.get());
  ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      *msg.meta, *parsed_casted->meta));
  ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      *std::get<1>(msg.msg_or_buffer),
      *std::get<1>(parsed_casted->msg_or_buffer)));
}

}  // namespace flare::protobuf

FLARE_TEST_MAIN
