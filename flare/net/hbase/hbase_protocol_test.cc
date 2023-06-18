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

#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"

#include "flare/base/down_cast.h"
#include "flare/init/on_init.h"
#include "flare/net/hbase/call_context.h"
#include "flare/net/hbase/hbase_client_controller.h"
#include "flare/net/hbase/hbase_client_protocol.h"
#include "flare/net/hbase/hbase_server_controller.h"
#include "flare/net/hbase/hbase_server_protocol.h"
#include "flare/net/hbase/message.h"
#include "flare/testing/echo_service.flare.pb.h"
#include "flare/testing/main.h"

namespace flare::hbase {

struct Dummy : testing::EchoService {
} dummy;

FLARE_ON_INIT(10, [] {
  HbaseServerProtocol::RegisterService(dummy.GetDescriptor());
});

namespace {

std::size_t GetWriteDelimitedSize(const google::protobuf::Message& msg) {
  return msg.ByteSizeLong() +
         google::protobuf::io::CodedOutputStream::VarintSize32(
             msg.ByteSizeLong());
}

template <class T>
std::string WriteToString(const T& hbase_msg) {
  NoncontiguousBufferBuilder nbb;
  hbase_msg.WriteTo(&nbb);
  return FlattenSlow(nbb.DestructiveGet());
}

}  // namespace

TEST(HbaseProtocol, ClientToServer) {
  HbaseClientProtocol client_protocol;
  HbaseServerProtocol server_protocol;

  ConnectionHeader conn_header;
  conn_header.set_service_name("EchoService");
  conn_header.set_cell_block_codec_class("my codec");
  client_protocol.InitializeHandshakeConfig(conn_header);

  testing::EchoRequest body;
  body.set_body("hello there.");

  HbaseRequest mine;
  mine.cell_block = CreateBufferSlow("some cell block data.");
  mine.header.set_method_name("Echo");
  mine.header.set_call_id(123);
  mine.header.set_request_param(true);
  mine.header.mutable_cell_block_meta()->set_length(mine.cell_block.ByteSize());
  mine.body = &body;

  for (int i = 0; i != 10; ++i) {
    NoncontiguousBuffer buffer;

    ProactiveCallContext client_ctx;
    client_protocol.WriteMessage(mine, &buffer, &client_ctx);
    if (i == 0) {
      client_protocol.handshake_done_ = true;
      auto was_size = buffer.ByteSize();
      auto buffer_cp = buffer;
      auto handshake_header_size =
          sizeof(HbaseHandshakeHeader) + conn_header.ByteSizeLong();

      // `buffer` contains partial handshake data.
      buffer.Clear();
      for (int j = 0; j != handshake_header_size - 1; ++j) {
        buffer.Append(buffer_cp.Cut(1));
        ASSERT_NE(StreamProtocol::MessageCutStatus::Cut,
                  server_protocol.TryCutMessage(&buffer, nullptr));
      }

      // `buffer` now contains a complete handshake header, and a partial
      // message. Handshake data will be cut off by `TryCutMessage` below.
      buffer.Append(buffer_cp.Cut(2));
      ASSERT_EQ(StreamProtocol::MessageCutStatus::NeedMore,
                server_protocol.TryCutMessage(&buffer, nullptr));
      ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
          conn_header, server_protocol.conn_header_));

      buffer.Append(std::move(buffer_cp));  // Now `buffer` contains a complete
                                            // message w/o connection header.
      ASSERT_EQ(handshake_header_size, was_size - buffer.ByteSize());
      ASSERT_EQ(buffer.ByteSize(),
                4 /* <Total Length> */ + GetWriteDelimitedSize(mine.header) +
                    GetWriteDelimitedSize(body) + mine.cell_block.ByteSize());
    }

    auto server_ctx_raw = server_protocol.GetControllerFactory()->Create(false);
    std::unique_ptr<Message> cut;
    ASSERT_EQ(StreamProtocol::MessageCutStatus::Cut,
              server_protocol.TryCutMessage(&buffer, &cut));
    ASSERT_TRUE(server_protocol.TryParse(&cut, server_ctx_raw.get()));
    auto&& yours = *cast<HbaseRequest>(cut.get());
    auto server_ctx = cast<PassiveCallContext>(server_ctx_raw.get());

    ASSERT_EQ("my codec", server_ctx->conn_header->cell_block_codec_class());
    ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
        mine.header, yours.header));
    ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
        *std::get<1>(mine.body), *std::get<0>(yours.body).Get()));
    ASSERT_EQ(FlattenSlow(mine.cell_block), FlattenSlow(yours.cell_block));
  }
}

TEST(HbaseProtocol, ServerToClient) {
  HbaseServerProtocol server_protocol;
  HbaseClientProtocol client_protocol;

  testing::EchoResponse body;
  body.set_body("hey there.");

  HbaseResponse mine;
  mine.cell_block = CreateBufferSlow("my cell block.");
  mine.header.set_call_id(10);
  mine.header.mutable_cell_block_meta()->set_length(mine.cell_block.ByteSize());
  mine.body = &body;

  NoncontiguousBuffer buffer;

  auto server_ctx = server_protocol.GetControllerFactory()->Create(false);
  server_protocol.WriteMessage(mine, &buffer, server_ctx.get());

  HbaseClientController client_ctlr;
  ProactiveCallContext client_ctx;
  testing::EchoResponse client_body;
  client_ctx.response_ptr = &client_body;
  client_ctx.client_controller = &client_ctlr;
  std::unique_ptr<Message> cut;
  ASSERT_EQ(StreamProtocol::MessageCutStatus::Cut,
            client_protocol.TryCutMessage(&buffer, &cut));
  ASSERT_TRUE(client_protocol.TryParse(&cut, &client_ctx));
  auto&& yours = *cast<HbaseResponse>(cut.get());

  ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(mine.header,
                                                                 yours.header));
  ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      *std::get<1>(mine.body), *std::get<0>(yours.body).Get()));
  ASSERT_EQ(FlattenSlow(mine.cell_block), FlattenSlow(yours.cell_block));
}

TEST(HbaseProtocol, ServerToClientException) {
  HbaseServerProtocol server_protocol;
  HbaseClientProtocol client_protocol;

  HbaseResponse mine;
  mine.header.set_call_id(10);
  mine.header.mutable_exception()->set_exception_class_name("xcpt class");
  mine.body = nullptr;

  NoncontiguousBuffer buffer;

  auto server_ctx = server_protocol.GetControllerFactory()->Create(false);
  server_protocol.WriteMessage(mine, &buffer, server_ctx.get());

  HbaseClientController client_ctlr;
  ProactiveCallContext client_ctx;
  NoncontiguousBuffer msg_cut;
  client_ctx.response_ptr = nullptr;
  client_ctx.client_controller = &client_ctlr;
  std::unique_ptr<Message> cut;
  ASSERT_EQ(StreamProtocol::MessageCutStatus::Cut,
            client_protocol.TryCutMessage(&buffer, &cut));
  ASSERT_TRUE(client_protocol.TryParse(&cut, &client_ctx));

  ASSERT_TRUE(client_ctlr.Failed());
  ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      mine.header.exception(), client_ctlr.GetException()));
}

}  // namespace flare::hbase

FLARE_TEST_MAIN
