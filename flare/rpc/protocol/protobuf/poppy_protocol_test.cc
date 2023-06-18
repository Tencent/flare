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

#include "flare/rpc/protocol/protobuf/poppy_protocol.h"

#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"

#include "flare/base/down_cast.h"
#include "flare/base/endian.h"
#include "flare/init/on_init.h"
#include "flare/rpc/protocol/protobuf/call_context.h"
#include "flare/rpc/protocol/protobuf/message.h"
#include "flare/rpc/protocol/protobuf/poppy_rpc_meta.pb.h"
#include "flare/rpc/protocol/protobuf/service_method_locator.h"
#include "flare/testing/echo_service.flare.pb.h"
#include "flare/testing/main.h"

namespace flare::protobuf {

struct Dummy : testing::EchoService {
} dummy;

// We use a lower priority here since method provider (i.e., listeners to
// `AddServiceForXxx`) are registered with priority 0.
FLARE_ON_INIT(10, [] {
  ServiceMethodLocator::Instance()->AddService(dummy.GetDescriptor());
});

// Server-side only. For client-side protocol there's no need to identify wire
// format.
TEST(PoppyProtocol, IdentifyWireFormat) {
  PoppyProtocol protocol(true);

  NoncontiguousBuffer buffer = CreateBufferSlow("POST");
  std::unique_ptr<Message> msg;

  EXPECT_EQ(PoppyProtocol::MessageCutStatus::NotIdentified,
            protocol.TryCutMessage(&buffer, &msg));
  buffer = CreateBufferSlow("something not looks right but still long enough");
  EXPECT_EQ(PoppyProtocol::MessageCutStatus::ProtocolMismatch,
            protocol.TryCutMessage(&buffer, &msg));
  buffer = CreateBufferSlow(
      "POST /__rpc_service__ HTTP/1.1\r\n"
      "Cookie: POPPY_AUTH_TICKET=\r\n"
      "X-Poppy-Compress-Type: 0,1\r\n"
      "X-Poppy-Tos: 96\r\n\r\n");
  EXPECT_EQ(PoppyProtocol::MessageCutStatus::NeedMore,
            protocol.TryCutMessage(&buffer, &msg));
}

TEST(PoppyProtocol, FromBytes) {
  PoppyProtocol protocol(true);
  poppy::RpcMeta meta;
  meta.set_sequence_id(10);
  meta.set_method("flare.testing.EchoService.Echo");
  testing::EchoRequest req;
  req.set_body("123");

  NoncontiguousBufferBuilder builder;
  builder.Append(
      "POST /__rpc_service__ HTTP/1.1\r\n"
      "Cookie: POPPY_AUTH_TICKET=\r\n"
      "X-Poppy-Compress-Type: 0,1\r\n"
      "X-Poppy-Tos: 96\r\n\r\n");
  auto meta_size = ToBigEndian<std::uint32_t>(meta.ByteSizeLong());
  auto body_size = ToBigEndian<std::uint32_t>(req.ByteSizeLong());
  builder.Append(&meta_size, 4);
  builder.Append(&body_size, 4);
  builder.Append(meta.SerializeAsString());
  builder.Append(req.SerializeAsString());

  auto buffer = builder.DestructiveGet();
  std::unique_ptr<Message> msg;
  auto ctlr = protocol.GetControllerFactory()->Create(false);
  ASSERT_EQ(PoppyProtocol::MessageCutStatus::Cut,
            protocol.TryCutMessage(&buffer, &msg));
  ASSERT_TRUE(protocol.TryParse(&msg, ctlr.get()));

  auto proto_msg = cast<ProtoMessage>(*msg);
  EXPECT_EQ(10, proto_msg->meta->correlation_id());
  EXPECT_EQ("flare.testing.EchoService.Echo",
            proto_msg->meta->request_meta().method_name());
  EXPECT_TRUE(proto_msg->attachment.Empty());

  auto body_msg_ptr = std::get<1>(proto_msg->msg_or_buffer).Get();
  ASSERT_EQ(std::type_index(typeid(req)),
            std::type_index(typeid(*body_msg_ptr)));
  EXPECT_EQ("123",
            flare::down_cast<testing::EchoRequest>(body_msg_ptr)->body());
}

TEST(PoppyProtocol, ClientToServer) {
  auto src = object_pool::Get<rpc::RpcMeta>();
  src->set_correlation_id(1);
  src->set_method_type(rpc::METHOD_TYPE_SINGLE);
  src->mutable_request_meta()->set_method_name(
      "flare.testing.EchoService.Echo");
  src->mutable_request_meta()->set_acceptable_compression_algorithms(
      1 << rpc::COMPRESSION_ALGORITHM_NONE |
      1 << rpc::COMPRESSION_ALGORITHM_SNAPPY);
  auto src_cp = *src;
  testing::EchoRequest payload;
  payload.set_body("asdf");

  PoppyProtocol client_prot(false), server_prot(true);
  ProtoMessage msg(std::move(src), MaybeOwning(non_owning, &payload));
  NoncontiguousBuffer buffer;
  ProactiveCallContext pcc;
  pcc.accept_response_in_bytes = false;
  pcc.method = dummy.GetDescriptor()->FindMethodByName("Echo");
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

TEST(PoppyProtocol, ServerToClient) {
  auto src = object_pool::Get<rpc::RpcMeta>();
  src->set_correlation_id(1);
  src->set_method_type(rpc::METHOD_TYPE_SINGLE);
  src->mutable_response_meta()->set_status(rpc::STATUS_OVERLOADED);
  src->mutable_response_meta()->set_description("Not yet overloaded, really");
  auto src_cp = *src;
  testing::EchoResponse payload;
  payload.set_body("abcd");

  PoppyProtocol server_prot(true), client_prot(false);
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
