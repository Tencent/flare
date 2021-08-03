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

#include <chrono>
#include <string>
#include <typeinfo>

#include "gtest/gtest.h"
#include "jsoncpp/json.h"

#include "flare/base/buffer.h"
#include "flare/base/deferred.h"
#include "flare/base/down_cast.h"
#include "flare/base/encoding/hex.h"
#include "flare/base/experimental/uuid.h"
#include "flare/base/internal/curl.h"
#include "flare/base/string.h"
#include "flare/init/override_flag.h"
#include "flare/net/http/http_client.h"
#include "flare/net/http/packet_desc.h"
#include "flare/rpc/binlog/packet_desc.h"
#include "flare/rpc/binlog/tags.h"
#include "flare/rpc/binlog/util/proto_binlog.pb.h"
#include "flare/rpc/binlog/util/proto_dry_runner.h"
#include "flare/rpc/http_handler.h"
#include "flare/rpc/protocol/http/binlog.pb.h"
#include "flare/rpc/server.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"

using namespace std::literals;

FLARE_OVERRIDE_FLAG(flare_binlog_dry_runner, "dummy");

namespace flare::http {

namespace {

SerializedServerPacket CreateServerCall() {
  SerializedServerPacket call;
  call.set_method(static_cast<int>(HttpMethod::Post));
  call.set_uri("/fancy-hello");
  call.set_version(static_cast<int>(HttpVersion::V_1_1));
  auto& h = *call.add_headers();
  h.set_key("Content-Type");
  h.set_value("application/json");
  call.set_body("hi there");
  return call;
}

struct Log {
  binlog::proto::Call incoming_call;
  std::vector<binlog::proto::Call> outgoing_calls;
};

Log CreateNewLog() {
  Log result;
  auto&& incoming = result.incoming_call;

  incoming.set_correlation_id("1");
  // @sa: `http::Service::GetUuid()`.
  (*incoming.mutable_system_tags())[binlog::tags::kHandlerUuid] =
      experimental::Uuid("FF754BCC-3E51-4ECB-8DE4-67F6A4A6AA61").ToString();
  incoming.add_incoming_pkts()->set_system_context(
      CreateServerCall().SerializeAsString());

  return result;
}

class DummyIncomingCall : public binlog::ProtoDryRunIncomingCall {
 public:
  void SetResultingJsonPtr(Json::Value* result_ptr) {
    result_ptr_ = result_ptr;
  }

  void CaptureOutgoingPacket(const binlog::PacketDesc& packet) override {
    FLARE_LOG_ERROR("asdf");
    if (auto pkt = dyn_cast<PacketDesc>(packet)) {
      (*result_ptr_)["resp_pkt"] = *std::get<1>(pkt->message)->body();
    } else {
      FLARE_CHECK(!"Unexpected");
    }
  }

 private:
  Json::Value* result_ptr_{};
};

// Represents an outgoing call.
class DummyOutgoingCall : public binlog::ProtoDryRunOutgoingCall {
 public:
  void CaptureOutgoingPacket(const binlog::PacketDesc& packet) override {
    // Nothing yet.
  }
};

class DummyDryRunContext : public binlog::DryRunContext {
 public:
  DummyDryRunContext() {
    auto log = CreateNewLog();
    incoming_.Init(log.incoming_call);
    incoming_.SetResultingJsonPtr(&result_);
  }

  binlog::DryRunIncomingCall* GetIncomingCall() override { return &incoming_; }

  Expected<binlog::DryRunOutgoingCall*, Status> TryGetOutgoingCall(
      const std::string& correlation_id) override {
    return &outgoings_.at(EncodeHex(correlation_id));
  }

  void SetInvocationStatus(std::string s) override { EXPECT_EQ("200", s); }

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

class DummyDryRunner : public binlog::DryRunner {
 public:
  ByteStreamParseStatus ParseByteStream(
      NoncontiguousBuffer* buffer,
      std::unique_ptr<binlog::DryRunContext>* context) override {
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
      "/fancy-hello",
      NewHttpPostHandler([](auto&& req, auto&& resp, auto&& ctx) {
        EXPECT_EQ("hi there", *req.body());
        resp->set_body("echo-ing: " + *req.body());
      }));
  server.Start();

  // NOT using `HttpClient` as it can be affected by `flare_binlog_dry_runner`
  // as well.
  auto result =
      flare::internal::HttpGet("http://" + listening_on.ToString(), 10s);
  ASSERT_TRUE(result);
  FLARE_LOG_INFO("{}", *result);
  Json::Reader reader;
  Json::Value value;
  ASSERT_TRUE(reader.parse(*result, value));
  EXPECT_EQ("echo-ing: hi there", value["resp_pkt"].asString());
}

}  // namespace flare::http

FLARE_TEST_MAIN
