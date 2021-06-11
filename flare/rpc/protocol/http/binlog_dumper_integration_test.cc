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

#include "googletest/gtest/gtest.h"

#include "flare/base/down_cast.h"
#include "flare/base/string.h"
#include "flare/init/override_flag.h"
#include "flare/net/http/http_client.h"
#include "flare/net/http/packet_desc.h"
#include "flare/rpc/binlog/packet_desc.h"
#include "flare/rpc/binlog/util/easy_dumping_log.h"
#include "flare/rpc/binlog/util/proto_binlog.pb.h"
#include "flare/rpc/binlog/util/proto_dumper.h"
#include "flare/rpc/http_handler.h"
#include "flare/rpc/server.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"

using namespace std::literals;

FLARE_OVERRIDE_FLAG(flare_binlog_dumper, "dummy");
FLARE_OVERRIDE_FLAG(flare_binlog_dumper_sampling_every_n, 1);

namespace flare {

namespace {

std::string CapturePacket(const binlog::PacketDesc& packet) {
  auto pkt = cast<http::PacketDesc>(packet);
  return pkt->ToString();
}

class DummyCall : public binlog::ProtoDumpingCall {
 protected:
  void CaptureIncomingPacket(
      const binlog::PacketDesc& packet,
      experimental::LazyEval<std::any>* dumper_ctx,
      experimental::LazyEval<std::string>* prov_ctx) override {
    *prov_ctx = CapturePacket(packet);
  }
  void CaptureOutgoingPacket(
      const binlog::PacketDesc& packet,
      experimental::LazyEval<std::any>* dumper_ctx,
      experimental::LazyEval<std::string>* prov_ctx) override {
    *prov_ctx = CapturePacket(packet);
  }
};

class DummyDumper;

// Represents an entire RPC log being dumped.
class DummyLog : public binlog::EasyDumpingLog<DummyCall> {
 public:
  explicit DummyLog(DummyDumper* dumper) : dumper_(dumper) {}
  void Dump() override;

 private:
  DummyDumper* dumper_;
};

struct Log {
  binlog::proto::Call incoming_call;
  std::vector<binlog::proto::Call> outgoing_calls;
};

class DummyDumper : public binlog::Dumper {
 public:
  std::unique_ptr<binlog::DumpingLog> StartDumping() override {
    std::scoped_lock _(lock_);
    return std::make_unique<DummyLog>(this);
  }

  void Dump(const Log& log) {
    std::scoped_lock _(lock_);
    is_dumped_ = true;
    log_ = log;
  }

  Log GetLog() const {
    std::scoped_lock _(lock_);
    return log_;
  }

  void Reset() {
    std::scoped_lock _(lock_);
    log_ = Log();
    is_dumped_ = false;
  }

  bool IsDumped() const noexcept {
    std::scoped_lock _(lock_);
    return is_dumped_;
  }

 private:
  mutable std::mutex lock_;
  Log log_;
  bool is_dumped_{};
};

void DummyLog::Dump() {
  Log log;
  log.incoming_call = incoming_call_.GetMessage();
  for (auto&& e : outgoing_calls_) {
    log.outgoing_calls.emplace_back(e.GetMessage());
  }
  dumper_->Dump(log);
}

FLARE_RPC_BINLOG_REGISTER_DUMPER("dummy", [] {
  return std::make_unique<DummyDumper>();
});

}  // namespace

TEST(Http, DumperIntegrationTest) {
  auto dumper = down_cast<DummyDumper>(binlog::GetDumper());
  auto listening_on = testing::PickAvailableEndpoint();
  dumper->Reset();

  Server server;
  server.AddHttpHandler(
      "/simple-get",
      NewHttpPostHandler([](auto&& req, auto&& resp, auto&& ctx) {
        resp->set_body("echo-ing: " + *req.body());
      }));
  server.ListenOn(listening_on);
  server.AddProtocol("http");
  server.Start();

  HttpClient client;
  auto result =
      client.Post(Format("http://{}/simple-get", listening_on.ToString()),
                  "body", HttpClient::RequestOptions{});
  ASSERT_TRUE(result);
  EXPECT_EQ("echo-ing: body", *result->body());

  while (!dumper->IsDumped()) {
  }

  auto log = dumper->GetLog();

  {
    auto expected_request = Format(
        "POST /simple-get HTTP/1.1\r\n"
        "Host: {}\r\n"
        "Accept: */*\r\n"
        "Content-Length: 4\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "\r\n"
        "body",
        listening_on.ToString());
    auto expected_resp =
        "HTTP/1.1 200 OK\r\n"
        "Connection: keep-alive\r\n"
        "Content-Length: 14\r\n"
        "\r\n"
        "echo-ing: body";
    EXPECT_EQ(expected_request,
              log.incoming_call.incoming_pkts(0).provider_context());
    EXPECT_EQ(expected_resp,
              log.incoming_call.outgoing_pkts(0).provider_context());
  }
}

}  // namespace flare

FLARE_TEST_MAIN
