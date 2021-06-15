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

#include "flare/rpc/binlog/dry_runner.h"

#include <chrono>
#include <string>
#include <typeinfo>

#include "gflags/gflags.h"
#include "gtest/gtest.h"
#include "jsoncpp/json.h"

#include "flare/base/buffer.h"
#include "flare/base/crypto/blake3.h"
#include "flare/base/deferred.h"
#include "flare/base/down_cast.h"
#include "flare/base/encoding/hex.h"
#include "flare/base/experimental/uuid.h"
#include "flare/base/internal/curl.h"
#include "flare/base/string.h"
#include "flare/init/override_flag.h"
#include "flare/net/http/dry_run_channel.h"
#include "flare/net/http/http_client.h"
#include "flare/net/http/packet_desc.h"
#include "flare/rpc/binlog/packet_desc.h"
#include "flare/rpc/binlog/tags.h"
#include "flare/rpc/binlog/util/proto_dry_runner.h"
#include "flare/rpc/http_handler.h"
#include "flare/rpc/protocol/http/binlog.pb.h"
#include "flare/rpc/protocol/protobuf/rpc_meta.pb.h"
#include "flare/rpc/rpc_channel.h"
#include "flare/rpc/rpc_client_controller.h"
#include "flare/rpc/rpc_server_controller.h"
#include "flare/rpc/server.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"

using namespace std::literals;

DECLARE_string(flare_binlog_dry_runner);

FLARE_OVERRIDE_FLAG(flare_binlog_dry_runner, "dummy");

namespace flare::binlog {

namespace {

const char* kDummyHttpResponseBody = "dummy echo body";
const char* kClientEchoRequestBody = "echo-req body";
const char* kServerEchoResponseBody = "echo-resp body123";

http::SerializedServerPacket CreateServerCall() {
  http::SerializedServerPacket packet;
  packet.set_uri("/whatever");
  auto&& h = packet.add_headers();
  h->set_key("connection");
  h->set_value("keep-alive");
  packet.set_method(static_cast<int32_t>(HttpMethod::Post));
  return packet;
}

http::SerializedClientPacket CreateClientCall() {
  http::SerializedClientPacket call;
  call.set_status(static_cast<uint32_t>(HttpStatus::OK));
  call.set_version(static_cast<uint32_t>(HttpVersion::V_1_1));
  call.set_body(kDummyHttpResponseBody);
  return call;
}

struct Log {
  proto::Call incoming_call;
  std::vector<proto::Call> outgoing_calls;
};

Log CreateNewLog() {
  Log result;
  auto&& incoming = result.incoming_call;
  auto&& outgoing = result.outgoing_calls.emplace_back();

  incoming.set_correlation_id("1");
  // @sa: `http::Service::GetUuid()`.
  (*incoming.mutable_system_tags())[tags::kHandlerUuid] =
      experimental::Uuid("FF754BCC-3E51-4ECB-8DE4-67F6A4A6AA61").ToString();

  incoming.add_incoming_pkts()->set_system_context(
      CreateServerCall().SerializeAsString());

  // @sa: `RpcChannel::GetBinlogCorrelationId`.
  outgoing.set_correlation_id(
      EncodeHex(Blake3(Format("Http-{}-{}-{}", "url", 1, ""))));

  outgoing.add_outgoing_pkts()->set_time_since_start(0);
  (*outgoing.mutable_system_tags())[tags::kInvocationStatus] = "0";
  auto&& resp = *outgoing.add_incoming_pkts();
  resp.set_time_since_start(100ms / 1ns);
  resp.set_system_context(CreateClientCall().SerializeAsString());

  return result;
}

class DummyIncomingCall : public ProtoDryRunIncomingCall {
 public:
  void SetResultingJsonPtr(Json::Value* result_ptr) {
    result_ptr_ = result_ptr;
  }

  void CaptureOutgoingPacket(const PacketDesc& packet) override {
    if (dyn_cast<http::PacketDesc>(packet)) {
      (*result_ptr_)["resp_pkt"] = kServerEchoResponseBody;
      return;
    }
    FLARE_UNEXPECTED("Not http packet");
  }

 private:
  Json::Value* result_ptr_{};
};

// Represents an outgoing call.
class DummyOutgoingCall : public ProtoDryRunOutgoingCall {
 public:
  void SetResultingJsonPtr(Json::Value* result_ptr) {
    result_ptr_ = result_ptr;
  }

  void CaptureOutgoingPacket(const PacketDesc& packet) override {
    if (dyn_cast<http::PacketDesc>(packet)) {
      (*result_ptr_)["http_outgoing"] = kClientEchoRequestBody;
      return;
    }
    FLARE_UNEXPECTED("Not http packet");
  }

 private:
  Json::Value* result_ptr_;
};

class DummyDryRunContext : public DryRunContext {
 public:
  DummyDryRunContext() {
    auto log = CreateNewLog();
    incoming_.Init(log.incoming_call);
    incoming_.SetResultingJsonPtr(&result_);
    for (auto&& e : log.outgoing_calls) {
      outgoings_[e.correlation_id()].Init(e);
      outgoings_[e.correlation_id()].SetResultingJsonPtr(&result_);
    }
  }

  DryRunIncomingCall* GetIncomingCall() override { return &incoming_; }

  Expected<DryRunOutgoingCall*, Status> TryGetOutgoingCall(
      const std::string& correlation_id) override {
    return &outgoings_.at(EncodeHex(Blake3(correlation_id)));
  }

  void SetInvocationStatus(std::string s) override {
    // Ignored.
  }

  void WriteReport(NoncontiguousBuffer* buffer) const override {
    auto str = result_.toStyledString();
    *buffer = CreateBufferSlow(Format(
        "HTTP/1.1 200 OK\r\nContent-Length: {}\r\n\r\n{}", str.size(), str));
  }

 private:
  DummyIncomingCall incoming_;
  std::unordered_map<std::string, DummyOutgoingCall> outgoings_;
  Json::Value result_;
};

class DummyDryRunner : public DryRunner {
 public:
  ByteStreamParseStatus ParseByteStream(
      NoncontiguousBuffer* buffer,
      std::unique_ptr<DryRunContext>* context) override {
    // Assuming no HTTP body present, which is the case in our test.
    auto bytes = FlattenSlowUntil(*buffer, "\r\n\r\n");
    if (!EndsWith(bytes, "\r\n\r\n")) {
      return ByteStreamParseStatus::NeedMore;
    }
    buffer->Skip(bytes.size());
    *context = std::make_unique<DummyDryRunContext>();
    return ByteStreamParseStatus::Success;
  }
};

FLARE_RPC_BINLOG_REGISTER_DRY_RUNNER("dummy", [] {
  return std::make_unique<DummyDryRunner>();
});

}  // namespace

TEST(DryRunner, All) {
  auto listening_on = testing::PickAvailableEndpoint();
  Server server;
  server.ListenOn(listening_on);
  server.AddProtocol("http");  // Doesn't matter, actually.
  server.AddHttpHandler(
      "/whatever", NewHttpPostHandler([](auto&& req, auto&& resp, auto&& ctx) {
        auto start = ReadSteadyClock();
        HttpClient client;
        auto&& outgoing_resp = client.Get("url");
        ASSERT_TRUE(outgoing_resp);
        EXPECT_EQ(kDummyHttpResponseBody, *outgoing_resp->body());
        resp->set_body(kServerEchoResponseBody);
        EXPECT_NEAR(100ms / 1ms, (ReadSteadyClock() - start) / 1ms, 20);
      }));
  server.Start();

  // NOT using `HttpClient` as it can be affected by `flare_binlog_dry_runner`
  // as well.
  auto result =
      *flare::internal::HttpGet("http://" + listening_on.ToString(), 10s);
  FLARE_LOG_INFO("{}", result);
  Json::Reader reader;
  Json::Value value;
  ASSERT_TRUE(reader.parse(result, value));

  EXPECT_EQ(kClientEchoRequestBody, value["http_outgoing"].asString());
  EXPECT_EQ(kServerEchoResponseBody, value["resp_pkt"].asString());
}

}  // namespace flare::binlog

FLARE_TEST_MAIN
