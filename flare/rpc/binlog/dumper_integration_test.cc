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

#include "flare/rpc/binlog/dumper.h"

#include <chrono>
#include <string>
#include <typeinfo>

#include "thirdparty/gflags/gflags.h"
#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/base/down_cast.h"
#include "flare/base/string.h"
#include "flare/init/override_flag.h"
#include "flare/rpc/binlog/packet_desc.h"
#include "flare/rpc/binlog/util/easy_dumping_log.h"
#include "flare/rpc/binlog/util/proto_binlog.pb.h"
#include "flare/rpc/binlog/util/proto_dumper.h"
#include "flare/rpc/rpc_channel.h"
#include "flare/rpc/rpc_client_controller.h"
#include "flare/rpc/rpc_server_controller.h"
#include "flare/rpc/server.h"
#include "flare/testing/echo_service.flare.pb.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"
#include "flare/testing/relay_service.flare.pb.h"

using namespace std::literals;

FLARE_OVERRIDE_FLAG(flare_binlog_dumper, "dummy");
FLARE_OVERRIDE_FLAG(flare_binlog_dumper_sampling_every_n, 1);

namespace flare::binlog {

namespace {

std::string CapturePacket(const PacketDesc& packet) {
  auto pkt = cast<ProtoPacketDesc>(packet);
  return FlattenSlow(pkt->WriteMessage()) + FlattenSlow(*pkt->attachment);
}

class DummyCall : public ProtoDumpingCall {
 protected:
  void CaptureIncomingPacket(
      const PacketDesc& packet, experimental::LazyEval<std::any>* dumper_ctx,
      experimental::LazyEval<std::string>* prov_ctx) override {
    *prov_ctx = CapturePacket(packet);
  }
  void CaptureOutgoingPacket(
      const PacketDesc& packet, experimental::LazyEval<std::any>* dumper_ctx,
      experimental::LazyEval<std::string>* prov_ctx) override {
    *prov_ctx = CapturePacket(packet);
  }
};

class DummyDumper;

// Represents an entire RPC log being dumped.
class DummyLog : public EasyDumpingLog<DummyCall> {
 public:
  explicit DummyLog(DummyDumper* dumper) : dumper_(dumper) {}
  void Dump() override;

 private:
  DummyDumper* dumper_;
};

struct Log {
  proto::Call incoming_call;
  std::vector<proto::Call> outgoing_calls;
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

static const char* kEchoReq = "echo-req";
static const char* kEchoResp = "echo-resp";
static const char* kRelayReq = "relay-req";
static const char* kRelayResp = "relay_resp";

class DummyRelay : public testing::SyncRelayService {
 public:
  void Relay(const testing::RelayRequest& request,
             testing::RelayResponse* response,
             RpcServerController* controller) override {
    if (controller->IsCapturingBinlog()) {
      EXPECT_FALSE(controller->GetBinlogCorrelationId().empty());
    }
    RpcChannel channel;
    FLARE_CHECK(channel.Open("flare://" + echo_server_at_.ToString()));
    testing::EchoRequest req;
    req.set_body(kEchoReq);
    testing::EchoService_SyncStub stub(&channel);
    RpcClientController ctlr;
    ctlr.SetRequestAttachment(controller->GetRequestAttachment());
    if (using_raw_bytes) {
      ctlr.SetRequestRawBytes(CreateBufferSlow(req.SerializeAsString()));
      ctlr.SetAcceptResponseRawBytes(true);
      if (!stub.Echo(req, &ctlr)) {
        controller->SetFailed("");
        return;
      }
      testing::EchoResponse resp;
      FLARE_CHECK(
          resp.ParseFromString(FlattenSlow(ctlr.GetResponseRawBytes())));
      EXPECT_EQ(kEchoResp, resp.body());
    } else {
      auto result = stub.Echo(req, &ctlr);
      if (!result) {
        controller->SetFailed("");
        return;
      }
      ASSERT_EQ(kEchoResp, result->body());
    }
    response->set_body(kRelayResp);
    controller->SetResponseAttachment(ctlr.GetResponseAttachment());
  }

  Endpoint echo_server_at_;
  std::atomic<bool> using_raw_bytes{};
};

class DummyEcho : public testing::SyncEchoService {
 public:
  void Echo(const testing::EchoRequest& request,
            testing::EchoResponse* response,
            RpcServerController* controller) override {
    if (fail_req_) {
      controller->SetFailed("");
      return;
    }
    if (abort_dump_) {
      controller->AbortBinlogCapture();
    }
    response->set_body(kEchoResp);
    controller->SetResponseAttachment(controller->GetRequestAttachment());
  }

  std::atomic<bool> abort_dump_{false};
  std::atomic<bool> fail_req_{false};
};

}  // namespace

class UsingRawBytesOrNot : public ::testing::TestWithParam<bool> {};

TEST_P(UsingRawBytesOrNot, Relay) {
  auto dumper = down_cast<DummyDumper>(GetDumper());
  auto listening_on = testing::PickAvailableEndpoint();
  dumper->Reset();

  DummyRelay relay;
  relay.echo_server_at_ = listening_on;
  relay.using_raw_bytes = GetParam();

  DummyEcho echo;
  // We don't want it to interfere with our testing with `rely`.
  echo.abort_dump_ = true;

  Server server;
  server.ListenOn(listening_on);
  server.AddProtocol("flare");
  server.AddService(&echo);
  server.AddService(&relay);
  server.Start();

  RpcChannel channel;
  FLARE_CHECK(channel.Open("flare://" + listening_on.ToString()));
  testing::RelayService_SyncStub stub(&channel);
  testing::RelayRequest req;
  RpcClientController ctlr;
  ctlr.SetRequestAttachment(CreateBufferSlow("attach-buf"));
  req.set_body(kRelayReq);
  auto result = stub.Relay(req, &ctlr);
  ASSERT_TRUE(result);
  EXPECT_EQ(kRelayResp, result->body());
  EXPECT_EQ("attach-buf", FlattenSlow(ctlr.GetResponseAttachment()));

  while (!dumper->IsDumped()) {
  }

  auto log = dumper->GetLog();

  {
    testing::EchoRequest echo_req;
    echo_req.set_body(kEchoReq);
    testing::EchoResponse echo_resp;
    echo_resp.set_body(kEchoResp);
    testing::RelayRequest relay_req;
    relay_req.set_body(kRelayReq);
    testing::RelayResponse relay_resp;
    relay_resp.set_body(kRelayResp);
    EXPECT_EQ(echo_req.SerializeAsString() + "attach-buf",
              log.outgoing_calls[0].outgoing_pkts(0).provider_context());
    EXPECT_EQ(echo_resp.SerializeAsString() + "attach-buf",
              log.outgoing_calls[0].incoming_pkts(0).provider_context());
    EXPECT_EQ(relay_req.SerializeAsString() + "attach-buf",
              log.incoming_call.incoming_pkts(0).provider_context());
    EXPECT_EQ(relay_resp.SerializeAsString() + "attach-buf",
              log.incoming_call.outgoing_pkts(0).provider_context());
  }
}

INSTANTIATE_TEST_SUITE_P(Dumper, UsingRawBytesOrNot,
                         ::testing::Values(false, true));

class DumperBasicTest : public ::testing::Test {
 public:
  void SetUp() override {
    dumper_ = down_cast<DummyDumper>(GetDumper());
    dumper_->Reset();

    server_ep_ = testing::PickAvailableEndpoint();
    echo_ = std::make_unique<DummyEcho>();
    relay_ = std::make_unique<DummyRelay>();
    server_ = std::make_unique<Server>();

    relay_->echo_server_at_ = server_ep_;

    server_->ListenOn(server_ep_);
    server_->AddProtocol("flare");
    server_->AddService(echo_.get());
    server_->AddService(relay_.get());
    server_->Start();

    echo_stub_ = std::make_unique<testing::EchoService_SyncStub>(
        "flare://" + server_ep_.ToString());
    relay_stub_ = std::make_unique<testing::RelayService_SyncStub>(
        "flare://" + server_ep_.ToString());
  }

  void TearDown() override { ctlr_.Reset(); }

 protected:
  Endpoint server_ep_;
  std::unique_ptr<DummyEcho> echo_;
  std::unique_ptr<DummyRelay> relay_;
  std::unique_ptr<Server> server_;
  std::unique_ptr<testing::EchoService_SyncStub> echo_stub_;
  std::unique_ptr<testing::RelayService_SyncStub> relay_stub_;
  RpcClientController ctlr_;

  DummyDumper* dumper_;
};

TEST_F(DumperBasicTest, Aborted) {
  echo_->abort_dump_ = true;

  ASSERT_TRUE(echo_stub_->Echo(testing::EchoRequest(), &ctlr_));
  std::this_thread::sleep_for(1s);  // Wait for DPC to run.
  ASSERT_FALSE(dumper_->IsDumped());
}

TEST_F(DumperBasicTest, FailedRpc) {
  echo_->fail_req_ = true;

  ASSERT_FALSE(relay_stub_->Relay(testing::RelayRequest(), &ctlr_));
  std::this_thread::sleep_for(1s);  // Wait for DPC to run.
  ASSERT_TRUE(dumper_->IsDumped());
}

}  // namespace flare::binlog

FLARE_TEST_MAIN
