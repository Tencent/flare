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

#include "flare/rpc/protocol/protobuf/svrkit_protocol.h"

#include <cstdint>
#include <string>

#include "thirdparty/googletest/gtest/gtest.h"
#include "thirdparty/protobuf/util/message_differencer.h"

#include "flare/base/encoding.h"
#include "flare/base/endian.h"
#include "flare/init/on_init.h"
#include "flare/rpc/protocol/protobuf/call_context.h"
#include "flare/rpc/protocol/protobuf/message.h"
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

struct [[gnu::packed]] MsgHeadExportOriginal {
  unsigned short magic;
  unsigned char version;
  unsigned char head_len;
  unsigned int body_len;
  unsigned short cmd_id;
  unsigned short head_chk_sum;
  unsigned int x_forward_for;
  unsigned char reserved2[4];
  unsigned int uin;
  int result;
  unsigned char reserved[4];
};

struct MsgSubHeadExportOriginal {
  unsigned int type;
  unsigned int length;
};

void DeserializeBytes(NoncontiguousBuffer bytes, std::string* body) {
  SvrkitProtocol server_prot(true);
  std::unique_ptr<Message> cut;
  ASSERT_EQ(StreamProtocol::MessageCutStatus::Cut,
            server_prot.TryCutMessage(&bytes, &cut));
  EXPECT_TRUE(bytes.Empty());

  PassiveCallContext passive_ctx;
  ASSERT_TRUE(server_prot.TryParse(&cut, &passive_ctx));
  *body = flare::down_cast<testing::EchoRequest>(
              std::get<1>(cast<ProtoMessage>(*cut)->msg_or_buffer).Get())
              ->body();
}

TEST(SvrkitProtocol, FromBytes) {
  testing::EchoRequest req;
  req.set_body("my body");

  // @sa: `testing/echo_service.proto` for `magic` and `cmd_id`.
  MsgHeadExportOriginal header = {
      .magic = ToBigEndian<std::uint16_t>(12345),
      .head_len = ToBigEndian<std::uint8_t>(32),
      .body_len = ToBigEndian<std::uint32_t>(req.ByteSizeLong()),
      .cmd_id = ToBigEndian<std::uint16_t>(1001),
      .reserved = {1, 0, 0, 0} /* `char`s, no byte order issue here. */};

  NoncontiguousBufferBuilder nbb;
  std::uint32_t total_size =
      sizeof(header) + FromBigEndian<std::uint32_t>(header.body_len);
  ToBigEndian(&total_size);
  nbb.Append(&total_size, sizeof(total_size));
  nbb.Append(&header, sizeof(header));
  nbb.Append(req.SerializeAsString());
  auto bytes = nbb.DestructiveGet();

  // Don't recognize the packet until it's full.
  for (int i = 0; i != bytes.ByteSize(); ++i) {
    auto copy = bytes;
    auto partial = copy.Cut(i);

    SvrkitProtocol server_prot(true);
    std::unique_ptr<Message> message;
    auto status = server_prot.TryCutMessage(&partial, &message);
    EXPECT_TRUE(status == StreamProtocol::MessageCutStatus::NeedMore ||
                status == StreamProtocol::MessageCutStatus::NotIdentified);
  }

  std::string body;
  DeserializeBytes(bytes, &body);
  EXPECT_EQ("my body", body);
}

TEST(SvrkitProtocol, FromBytesWithCookie) {
  std::string cookie = "my cookie";
  testing::EchoRequest req;
  req.set_body("my body");

  MsgHeadExportOriginal header = {
      .magic = ToBigEndian<std::uint16_t>(12345),
      .head_len = ToBigEndian<std::uint8_t>(32),
      .body_len = ToBigEndian<std::uint32_t>(
          sizeof(MsgSubHeadExportOriginal) * 2 + 6 /* "END" * 2 */ +
          req.ByteSizeLong() + cookie.size()),
      .cmd_id = ToBigEndian<std::uint16_t>(1001),
      .reserved = {1, 0, 1, 0} /* `char`s, no byte order issue here. */};
  MsgSubHeadExportOriginal seg_header1 = {
      .type = ToBigEndian<std::uint32_t>(1),
      .length = ToBigEndian<std::uint32_t>(req.ByteSizeLong() + 3)};
  MsgSubHeadExportOriginal seg_header2 = {
      .type = ToBigEndian<std::uint32_t>(2),
      .length = ToBigEndian<std::uint32_t>(cookie.size() + 3)};

  NoncontiguousBufferBuilder nbb;
  std::uint32_t total_size =
      sizeof(header) + FromBigEndian<std::uint32_t>(header.body_len);
  ToBigEndian(&total_size);
  nbb.Append(&total_size, sizeof(total_size));
  nbb.Append(&header, sizeof(header));
  nbb.Append(&seg_header1, sizeof(seg_header1));
  nbb.Append(req.SerializeAsString());
  nbb.Append("END");
  nbb.Append(&seg_header2, sizeof(seg_header2));
  nbb.Append(cookie);
  nbb.Append("END");

  std::string body;
  DeserializeBytes(nbb.DestructiveGet(), &body);
  EXPECT_EQ("my body", body);
}

TEST(SvrkitProtocol, FromBytesRaw) {
  // From `tcpdump`, with some changes (magic / cmd_id / cookie / body
  // content...).
  static const auto kBytes =
      "000001cf30390020000001af03e9000000000000000001001a193ba00000000001000100"
      "00000001000000b40aae0161616161616161616161616161616161616161616161616161"
      "616161616161616161616161616161616161616161616161616161616161616161616161"
      "616161616161616161616161616161616161616161616161616161616161616161616161"
      "616161616161616161616161616161616161616161616161616161616161616161616161"
      "616161616161616161616161616161616161616161616161616161616161616161616161"
      "6161616161454e4400000002000000ebbe04000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "0000000000000000000000000000000000000000000000000000000000000000454e44";

  std::string body;
  DeserializeBytes(CreateBufferSlow(*DecodeHex(kBytes)), &body);
  EXPECT_EQ(
      // What's encoded in `kBytes`.
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      body);
}

TEST(SvrkitProtocol, ClientToServer) {
  auto src = object_pool::Get<rpc::RpcMeta>();
  src->set_correlation_id(Message::kNonmultiplexableCorrelationId);
  src->set_method_type(rpc::METHOD_TYPE_SINGLE);
  src->mutable_request_meta()->set_acceptable_compression_algorithms(
      1 << rpc::COMPRESSION_ALGORITHM_NONE |
      1 << rpc::COMPRESSION_ALGORITHM_SNAPPY);
  src->mutable_request_meta()->set_method_name(
      "flare.testing.EchoService.Echo");
  auto src_cp = *src;
  testing::EchoRequest payload;
  payload.set_body("asdf");

  SvrkitProtocol client_prot(false), server_prot(true);
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
  ASSERT_EQ(StreamProtocol::MessageCutStatus::Cut,
            server_prot.TryCutMessage(&buffer, &parsed));
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

TEST(SvrkitProtocol, ServerToClient) {
  auto src = object_pool::Get<rpc::RpcMeta>();
  src->set_correlation_id(Message::kNonmultiplexableCorrelationId);
  src->set_method_type(rpc::METHOD_TYPE_SINGLE);
  src->mutable_response_meta()->set_status(rpc::STATUS_OVERLOADED);
  auto src_cp = *src;
  testing::EchoResponse payload;
  payload.set_body("abcd");

  SvrkitProtocol server_prot(true), client_prot(false);
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
