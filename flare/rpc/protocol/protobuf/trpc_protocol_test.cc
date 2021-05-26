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

#include "flare/rpc/protocol/protobuf/trpc_protocol.h"

#include "thirdparty/googletest/gtest/gtest.h"
#include "thirdparty/protobuf/util/message_differencer.h"

#include "flare/base/buffer.h"
#include "flare/base/endian.h"
#include "flare/init/on_init.h"
#include "flare/rpc/protocol/protobuf/call_context.h"
#include "flare/rpc/protocol/protobuf/message.h"
#include "flare/rpc/protocol/protobuf/service_method_locator.h"
#include "flare/rpc/protocol/protobuf/trpc.pb.h"
#include "flare/testing/echo_service.flare.pb.h"
#include "flare/testing/main.h"

namespace flare::protobuf {

auto service_desc = testing::EchoService::descriptor();

// We use a lower priority here since method provider (i.e., listeners to
// `AddServiceForXxx`) are registered with priority 0.
FLARE_ON_INIT(10, [] {
  ServiceMethodLocator::Instance()->AddService(service_desc);
});

struct [[gnu::packed]] TrpcHeader {
  std::uint16_t magic;
  std::uint8_t data_type;
  std::uint8_t stream_frame_type;
  std::uint32_t total_size;
  std::uint16_t header_size;
  std::uint32_t stream_id;
  std::uint16_t reserved;
};

TEST(TrpcProtocol, Json) {
  std::string_view json = R"({"body":"123","never_used_body":"234"})";
  trpc::RequestProtocol req;

  req.set_version(trpc::TRPC_PROTO_V1);
  req.set_call_type(trpc::TRPC_UNARY_CALL);
  req.set_request_id(1);
  req.set_func("/flare.testing.EchoService/Echo");
  req.set_content_type(trpc::TRPC_JSON_ENCODE);

  TrpcHeader header = {
      .magic = trpc::TRPC_MAGIC_VALUE,
      .data_type = trpc::TRPC_UNARY_FRAME,
      .stream_frame_type = trpc::TRPC_UNARY,
      .total_size = static_cast<std::uint32_t>(
          sizeof(TrpcHeader) + req.ByteSizeLong() + json.size()),
      .header_size = static_cast<std::uint16_t>(req.ByteSizeLong()),
      .stream_id = 0,
      .reserved = 0};

  // Fields in `TrpcHeader` is NOT naturally-aligned, therefore we can't swap
  // its endian inplace (to avoid unaligned-pointer.).
  header.magic = ToBigEndian<std::uint16_t>(header.magic);
  header.data_type = ToBigEndian<std::uint8_t>(header.data_type);
  header.stream_frame_type =
      ToBigEndian<std::uint8_t>(header.stream_frame_type);
  header.total_size = ToBigEndian<std::uint32_t>(header.total_size);
  header.header_size = ToBigEndian<std::uint16_t>(header.header_size);
  header.stream_id = ToBigEndian<std::uint32_t>(header.stream_id);
  header.reserved = ToBigEndian<std::uint16_t>(header.reserved);

  NoncontiguousBufferBuilder builder;
  builder.Append(&header, sizeof(header));
  builder.Append(req.SerializeAsString());
  builder.Append(json);

  auto buffer = builder.DestructiveGet();

  PassiveCallContext pcc;
  TrpcProtocol protocol(true);
  std::unique_ptr<Message> msg;

  // Decode from JSON.
  ASSERT_EQ(StreamProtocol::MessageCutStatus::Cut,
            protocol.TryCutMessage(&buffer, &msg));
  ASSERT_TRUE(protocol.TryParse(&msg, &pcc));
  auto echo_msg = flare::down_cast<testing::EchoRequest>(
      *std::get<1>(cast<ProtoMessage>(*msg)->msg_or_buffer));
  EXPECT_EQ("123", echo_msg->body());
  EXPECT_EQ("234", echo_msg->never_used_body());

  // Encode to JSON.
  NoncontiguousBuffer written;
  protocol.WriteMessage(*msg, &written, &pcc);
  auto flatten = FlattenSlow(written);
  auto body = flatten.substr(flatten.size() - json.size());
  EXPECT_TRUE(body == json);
}

TEST(TrpcProtocol, ClientToServer) {
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

  TrpcProtocol client_prot(false), server_prot(true);
  ProtoMessage msg(std::move(src), MaybeOwning(non_owning, &payload));
  NoncontiguousBuffer buffer;
  ProactiveCallContext pcc;
  pcc.accept_response_in_bytes = false;
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

TEST(TrpcProtocol, ServerToClient) {
  auto src = object_pool::Get<rpc::RpcMeta>();
  src->set_correlation_id(1);
  src->set_method_type(rpc::METHOD_TYPE_SINGLE);
  src->mutable_response_meta()->set_status(rpc::STATUS_OVERLOADED);
  auto src_cp = *src;
  testing::EchoResponse payload;
  payload.set_body("abcd");

  TrpcProtocol server_prot(true), client_prot(false);
  ProtoMessage msg(std::move(src), MaybeOwning(non_owning, &payload));
  NoncontiguousBuffer buffer;
  PassiveCallContext passive_ctx;
  passive_ctx.trpc_content_type = trpc::TRPC_PROTO_ENCODE;
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
