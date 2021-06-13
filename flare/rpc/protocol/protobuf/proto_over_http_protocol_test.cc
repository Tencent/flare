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

#include "flare/rpc/protocol/protobuf/proto_over_http_protocol.h"

#include <string>
#include <unordered_map>

#include "gtest/gtest.h"
#include "google/protobuf/util/message_differencer.h"

#include "flare/base/down_cast.h"
#include "flare/base/string.h"
#include "flare/init/on_init.h"
#include "flare/rpc/protocol/protobuf/call_context.h"
#include "flare/rpc/protocol/protobuf/message.h"
#include "flare/rpc/protocol/protobuf/service_method_locator.h"
#include "flare/testing/echo_service.flare.pb.h"
#include "flare/testing/main.h"

namespace flare::protobuf {

struct Dummy : testing::EchoService {
} dummy;

FLARE_ON_INIT(10, [] {
  ServiceMethodLocator::Instance()->AddService(dummy.GetDescriptor());
});

auto ParseHttpMessageLite(const std::string& msg) {
  std::string start_line;
  std::unordered_map<std::string, std::string> headers;
  std::string body;
  auto first_part = msg.substr(0, msg.find("\r\n\r\n"));
  auto splited = Split(first_part, "\r\n");
  start_line = std::string(splited[0]);
  splited.erase(splited.begin());
  for (auto&& e : splited) {
    auto kv = Split(e, ":");
    headers[std::string(Trim(kv[0]))] = std::string(Trim(kv[1]));
  }
  body = msg.substr(first_part.size() + 4);

  return std::tuple(start_line, headers, body);
}

TEST(ProtoOverHttpProtocol, PackAndParseHttpRequestMessage) {
  auto src = object_pool::Get<rpc::RpcMeta>();
  src->set_correlation_id(1);
  src->set_method_type(rpc::METHOD_TYPE_SINGLE);
  src->mutable_request_meta()->set_method_name(
      "flare.testing.EchoService.Echo");
  auto src_cp = *src;
  testing::EchoRequest payload;
  payload.set_body("asdf");

  ProtoOverHttpProtocol client_prot(
      ProtoOverHttpProtocol::ContentType::kApplicationJson, false);
  ProtoMessage msg(std::move(src), MaybeOwning(non_owning, &payload));
  ProactiveCallContext pcc;
  pcc.accept_response_in_bytes = false;
  pcc.method = dummy.GetDescriptor()->FindMethodByName("Echo");
  NoncontiguousBuffer buffer;
  client_prot.WriteMessage(msg, &buffer, &pcc);

  auto&& [start_line, headers, body] =
      ParseHttpMessageLite(FlattenSlow(buffer));

  EXPECT_EQ("POST /rpc/flare.testing.EchoService.Echo HTTP/1.1", start_line);
  EXPECT_EQ("application/json", headers["Content-Type"]);
  EXPECT_EQ("1", headers["Rpc-SeqNo"]);
  EXPECT_EQ("{\"body\":\"asdf\"}\n", body);
}

TEST(ProtoOverHttpProtocol, ParseHttpMessageFromStringApplicationJson) {
  constexpr std::string_view kContentTypes[] = {
      "application/json", "application/json; charset=utf-8",
      "application/json;charset=UTF-8", "application/json; charset=UTF-8",
      "application/json;   charset=UTF-8"};

  for (auto&& content_type : kContentTypes) {
    FLARE_LOG_INFO("Testing [{}].", content_type);

    std::string body = "{\"body\":\"asdf\"}";
    auto req_str = Format(
        "POST /rpc/flare.testing.EchoService.Echo HTTP/1.1\r\n"
        // Header names below are deliberately using "weird-looking" case.
        //
        // Some clients (notably Java) are using lower-cased header names, while
        // the rest prefer to `Camel-Case-With-Hyphen`.
        "rpc-Seqno: 123\r\n"
        "content-type: {}\r\n"
        "cOntent-Length: {}\r\n"
        "\r\n"
        "{}",
        content_type, body.size(), body);
    auto buffer = CreateBufferSlow(req_str);

    ProtoOverHttpProtocol server_prot(
        ProtoOverHttpProtocol::ContentType::kApplicationJson, true);
    auto passive_ctx = server_prot.GetControllerFactory()->Create(false);
    std::unique_ptr<Message> msg;
    ASSERT_EQ(StreamProtocol::MessageCutStatus::Cut,
              server_prot.TryCutMessage(&buffer, &msg));
    ASSERT_TRUE(server_prot.TryParse(&msg, passive_ctx.get()));

    auto msg_body = std::get<1>(cast<ProtoMessage>(*msg)->msg_or_buffer).Get();
    ASSERT_EQ("asdf", flare::down_cast<testing::EchoRequest>(msg_body)->body());
  }
}

TEST(ProtoOverHttpProtocol, ParseHttpMessageFromStringUnrecognized) {
  std::string body = "{\"body\":\"asdf\"}";
  auto req_str = Format(
      "POST /rpc/flare.testing.EchoService.Echo HTTP/1.1\r\n"
      "rpc-seqno: 123\r\n"
      "content-type: application/x-not-recognized-json\r\n"
      "content-Length: {}\r\n"
      "\r\n"
      "{}",
      body.size(), body);
  auto buffer = CreateBufferSlow(req_str);

  ProtoOverHttpProtocol server_prot(
      ProtoOverHttpProtocol::ContentType::kApplicationJson, true);
  auto passive_ctx = server_prot.GetControllerFactory()->Create(false);
  std::unique_ptr<Message> msg;
  ASSERT_EQ(StreamProtocol::MessageCutStatus::ProtocolMismatch,
            server_prot.TryCutMessage(&buffer, &msg));
}

TEST(ProtoOverHttpProtocol, PackAndParseHttpResponseMessage) {
  auto src = object_pool::Get<rpc::RpcMeta>();
  src->set_correlation_id(12345);
  src->set_method_type(rpc::METHOD_TYPE_SINGLE);
  src->mutable_response_meta()->set_status(rpc::STATUS_SUCCESS);
  // `meta.description` is ignored after serialization.
  src->mutable_response_meta()->set_description("great success.");
  auto src_cp = *src;
  testing::EchoResponse payload;
  payload.set_body("abcd");

  ProtoOverHttpProtocol server_prot(
      ProtoOverHttpProtocol::ContentType::kApplicationJson, true);
  ProtoMessage msg(std::move(src), MaybeOwning(non_owning, &payload));
  auto passive_ctx = server_prot.GetControllerFactory()->Create(false);
  NoncontiguousBuffer buffer;
  server_prot.WriteMessage(msg, &buffer, passive_ctx.get());

  auto&& [start_line, headers, body] =
      ParseHttpMessageLite(FlattenSlow(buffer));
  EXPECT_EQ("HTTP/1.1 200 OK", start_line);
  EXPECT_EQ("application/json", headers["Content-Type"]);
  EXPECT_EQ("12345", headers["Rpc-SeqNo"]);
  EXPECT_EQ("{\"body\":\"abcd\"}\n", body);
}

TEST(ProtoOverHttpProtocol, PackFailureHttpResponseMessage) {
  auto src = object_pool::Get<rpc::RpcMeta>();
  src->set_correlation_id(12345);
  src->set_method_type(rpc::METHOD_TYPE_SINGLE);
  src->mutable_response_meta()->set_status(rpc::STATUS_FAILED);
  src->mutable_response_meta()->set_description("core dumped.");

  ProtoOverHttpProtocol server_prot(
      ProtoOverHttpProtocol::ContentType::kApplicationJson, true);
  ProtoMessage msg(std::move(src), nullptr);
  auto passive_ctx = server_prot.GetControllerFactory()->Create(false);
  NoncontiguousBuffer buffer;
  server_prot.WriteMessage(msg, &buffer, passive_ctx.get());

  auto&& [start_line, headers, body] =
      ParseHttpMessageLite(FlattenSlow(buffer));
  EXPECT_EQ("HTTP/1.1 500 Internal Server Error", start_line);
  EXPECT_EQ(std::to_string(rpc::STATUS_FAILED), headers["Rpc-Error-Code"]);
  EXPECT_EQ("core dumped.", headers["Rpc-Error-Reason"]);
}

TEST(ProtoOverHttpProtocol, ClientToServer) {
  for (auto content_type :
       {ProtoOverHttpProtocol::ContentType::kApplicationJson,
        ProtoOverHttpProtocol::ContentType::kProto3Json,
        ProtoOverHttpProtocol::ContentType::kDebugString,
        ProtoOverHttpProtocol::ContentType::kProtobuf}) {
    auto src = object_pool::Get<rpc::RpcMeta>();
    src->set_correlation_id(1);
    src->set_method_type(rpc::METHOD_TYPE_SINGLE);
    src->mutable_request_meta()->set_method_name(
        "flare.testing.EchoService.Echo");
    auto src_cp = *src;
    testing::EchoRequest payload;
    payload.set_body("asdf");
    NoncontiguousBuffer bytes;

    ProtoOverHttpProtocol client_prot(content_type, false),
        server_prot(content_type, true);
    ProtoMessage msg(std::move(src), MaybeOwning(non_owning, &payload), {});
    ProactiveCallContext pcc;
    pcc.method = dummy.GetDescriptor()->FindMethodByName("Echo");
    client_prot.WriteMessage(msg, &bytes, &pcc);

    ASSERT_TRUE(
        google::protobuf::util::MessageDifferencer::Equals(*msg.meta, src_cp));
    ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
        *std::get<1>(msg.msg_or_buffer), payload));

    auto passive_ctx = server_prot.GetControllerFactory()->Create(false);
    std::unique_ptr<Message> cut;
    ASSERT_EQ(StreamProtocol::MessageCutStatus::Cut,
              server_prot.TryCutMessage(&bytes, &cut));
    EXPECT_TRUE(bytes.Empty());
    ASSERT_TRUE(server_prot.TryParse(&cut, passive_ctx.get()));

    // Same as the original one.
    auto parsed_casted = cast<ProtoMessage>(cut.get());
    ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
        *msg.meta, *parsed_casted->meta));
    ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
        *std::get<1>(msg.msg_or_buffer),
        *std::get<1>(parsed_casted->msg_or_buffer)));
  }
}

TEST(ProtoOverHttpProtocol, ServerToClient) {
  for (auto content_type :
       {ProtoOverHttpProtocol::ContentType::kApplicationJson,
        ProtoOverHttpProtocol::ContentType::kProto3Json,
        ProtoOverHttpProtocol::ContentType::kDebugString,
        ProtoOverHttpProtocol::ContentType::kProtobuf}) {
    auto src = object_pool::Get<rpc::RpcMeta>();
    src->set_correlation_id(1);
    src->set_method_type(rpc::METHOD_TYPE_SINGLE);
    src->mutable_response_meta()->set_status(rpc::STATUS_SUCCESS);
    auto src_cp = *src;
    testing::EchoResponse payload;
    payload.set_body("abcd");
    NoncontiguousBuffer bytes;

    ProtoOverHttpProtocol server_prot(content_type, true),
        client_prot(content_type, false);
    ProtoMessage msg(std::move(src), MaybeOwning(non_owning, &payload), {});
    auto passive_ctx = server_prot.GetControllerFactory()->Create(false);
    server_prot.WriteMessage(msg, &bytes, passive_ctx.get());

    ASSERT_TRUE(
        google::protobuf::util::MessageDifferencer::Equals(*msg.meta, src_cp));
    ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
        *std::get<1>(msg.msg_or_buffer), payload));

    std::unique_ptr<Message> cut;
    testing::EchoResponse unpack_to;
    ProactiveCallContext pcc;
    pcc.accept_response_in_bytes = false;
    pcc.expecting_stream = false;
    pcc.response_ptr = &unpack_to;
    ASSERT_TRUE(client_prot.TryCutMessage(&bytes, &cut) ==
                StreamProtocol::MessageCutStatus::Cut);
    EXPECT_TRUE(bytes.Empty());
    ASSERT_TRUE(client_prot.TryParse(&cut, &pcc));

    // Same as the original one.
    auto parsed_casted = cast<ProtoMessage>(cut.get());
    ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
        *msg.meta, *parsed_casted->meta));
    ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
        *std::get<1>(msg.msg_or_buffer),
        *std::get<1>(parsed_casted->msg_or_buffer)));
  }
}

TEST(ProtoOverHttpProtocol, MethodNotFound) {
  auto src = object_pool::Get<rpc::RpcMeta>();
  src->set_correlation_id(1);
  src->set_method_type(rpc::METHOD_TYPE_SINGLE);
  src->mutable_request_meta()->set_method_name("invalid-method-name");

  ProtoOverHttpProtocol client_prot(
      ProtoOverHttpProtocol::ContentType::kProtobuf, false),
      server_prot(ProtoOverHttpProtocol::ContentType::kProtobuf, true);
  ProtoMessage msg(std::move(src), std::make_unique<testing::EchoRequest>(),
                   {});
  ProactiveCallContext pcc;
  pcc.accept_response_in_bytes = false;
  pcc.method = dummy.GetDescriptor()->FindMethodByName("Echo");
  NoncontiguousBuffer bytes;
  client_prot.WriteMessage(msg, &bytes, &pcc);
  std::unique_ptr<Message> cut;
  ASSERT_EQ(StreamProtocol::MessageCutStatus::Cut,
            server_prot.TryCutMessage(&bytes, &cut));
  auto passive_ctx = server_prot.GetControllerFactory()->Create(false);
  ASSERT_TRUE(server_prot.TryParse(&cut, passive_ctx.get()));
  ASSERT_TRUE(!!dyn_cast<EarlyErrorMessage>(cut.get()));
}

}  // namespace flare::protobuf

FLARE_TEST_MAIN
