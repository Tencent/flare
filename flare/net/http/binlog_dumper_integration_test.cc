// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "flare/rpc/binlog/dumper.h"

#include <chrono>
#include <string>
#include <typeinfo>

#include "gflags/gflags.h"
#include "googletest/gtest/gtest.h"

#include "flare/base/string.h"
#include "flare/init/override_flag.h"
#include "flare/net/http/http_client.h"
#include "flare/rpc/binlog/testing.h"
#include "flare/rpc/binlog/util/easy_dumping_log.h"
#include "flare/rpc/http_handler.h"
#include "flare/rpc/internal/session_context.h"
#include "flare/rpc/rpc_client_controller.h"
#include "flare/rpc/server.h"
#include "flare/testing/echo_service.flare.pb.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"

using namespace std::literals;

FLARE_OVERRIDE_FLAG(flare_binlog_dumper, "dummy");
FLARE_OVERRIDE_FLAG(flare_binlog_dumper_sampling_every_n, 1);

namespace flare::binlog {

namespace {

class DummyCall : public IdentityDumpingCall {
 protected:
  void CaptureIncomingPacket(
      const PacketDesc& packet, experimental::LazyEval<std::any>* dumper_ctx,
      experimental::LazyEval<std::string>* prov_ctx) override {
    *dumper_ctx = packet.Describe();
  }
  void CaptureOutgoingPacket(
      const PacketDesc& packet, experimental::LazyEval<std::any>* dumper_ctx,
      experimental::LazyEval<std::string>* prov_ctx) override {
    *dumper_ctx = packet.Describe();
  }
};

class DummyDumper;

// Represents an entire RPC log being dumped.
class DummyLog : public EasyDumpingLog<NullDumpingCall, DummyCall> {
 public:
  explicit DummyLog(DummyDumper* dumper) : dumper_(dumper) {}
  void Dump() override;

 private:
  DummyDumper* dumper_;
};

struct Log {
  std::vector<std::string> sent;
};

class DummyDumper : public Dumper {
 public:
  std::unique_ptr<DumpingLog> StartDumping() override {
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

  {
    std::scoped_lock _(lock_);

    FLARE_CHECK_EQ(outgoing_calls_.size(), 1);
    for (auto&& e : outgoing_calls_) {
      FLARE_CHECK_EQ(e.GetOutgoingPackets().size(), 1);
      log.sent.push_back(
          std::any_cast<std::string>(e.GetOutgoingPackets()[0].dumper_context));
    }
  }
  dumper_->Dump(log);
}

FLARE_RPC_BINLOG_REGISTER_DUMPER("dummy", [] {
  return std::make_unique<DummyDumper>();
});

class DummyEcho : public testing::SyncEchoService {
 public:
  DummyEcho(const Endpoint& ep) : ep_(ep) {}
  void Echo(const testing::EchoRequest& request,
            testing::EchoResponse* response,
            RpcServerController* controller) override {
    HttpClient client;
    client.Post("http://" + ep_.ToString() + "/http_server", "123", {});
  }

 private:
  Endpoint ep_;
};

}  // namespace

TEST(BinlogIntegrationTest, All) {
  auto listening_on = testing::PickAvailableEndpoint();

  Server server;
  server.ListenOn(listening_on);
  server.AddProtocol("flare");
  server.AddService(std::make_unique<DummyEcho>(listening_on));
  server.AddProtocol("http");
  server.AddHttpHandler(
      "/http_server", NewHttpPostHandler([](auto&& req, auto* resp, auto&&) {
        // We don't want this one to inference our test.
        if (auto&& opt = rpc::session_context->binlog.dumper) {
          opt->Abort();
        }
      }));
  server.Start();

  testing::EchoService_SyncStub stub("flare://" + listening_on.ToString());
  RpcClientController ctlr;
  auto result = stub.Echo({}, &ctlr);
  ASSERT_TRUE(result);

  auto dumper = down_cast<DummyDumper>(GetDumper());
  while (!dumper->IsDumped()) {
  }
  auto log = dumper->GetLog();
  ASSERT_EQ(1, log.sent.size());
  EXPECT_EQ("123", log.sent[0].substr(log.sent[0].find("\r\n\r\n") + 4));
}

}  // namespace flare::binlog

FLARE_TEST_MAIN
