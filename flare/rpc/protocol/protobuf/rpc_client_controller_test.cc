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

#define FLARE_RPC_CLIENT_CONTROLLER_SUPPRESS_INCLUDE_WARNING

#include "flare/rpc/protocol/protobuf/rpc_client_controller.h"

#include <sys/signal.h>

#include <chrono>
#include <thread>

#include "googletest/gtest/gtest.h"

#include "flare/base/callback.h"
#include "flare/base/string.h"
#include "flare/fiber/this_fiber.h"
#include "flare/init/override_flag.h"
#include "flare/rpc/rpc_channel.h"
#include "flare/rpc/rpc_server_controller.h"
#include "flare/rpc/server.h"
#include "flare/testing/echo_service.flare.pb.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"

using namespace std::literals;

FLARE_OVERRIDE_FLAG(flare_rpc_client_default_streaming_rpc_timeout_ms, 5000);

namespace flare {

class ServiceImpl : public testing::EchoService {
 public:
  void Echo(google::protobuf::RpcController* controller,
            const testing::EchoRequest* request,
            testing::EchoResponse* response,
            google::protobuf::Closure* done) override {
    if (auto sleep_for = TryParse<int>(request->body())) {
      this_fiber::SleepFor(*sleep_for * 1ms);
    }
    response->set_body(request->body());
    auto ctlr = flare::down_cast<RpcServerController>(controller);
    ASSERT_NE("", ctlr->GetRemotePeer().ToString());
    done->Run();
  }

  void EchoStreamResponse(google::protobuf::RpcController* controller,
                          const testing::EchoRequest* request,
                          testing::EchoResponse* response,
                          google::protobuf::Closure* done) override {
    this_fiber::SleepFor(10s);
    auto ctlr = flare::down_cast<RpcServerController>(controller);
    ctlr->GetStreamWriter<testing::EchoResponse>().Close();
    done->Run();
  }
};

void InitServer(Server* server, const Endpoint& ep) {}

TEST(RpcClientController, Basics) {
  RpcClientController rc;

  rc.SetMaxRetries(1);
  ASSERT_EQ(1, rc.GetMaxRetries());

  auto t = ReadSteadyClock();
  rc.SetTimeout(t);
  ASSERT_EQ(t, rc.GetTimeout());

  auto x = 0;
  rc.SetCompletion(flare::NewCallback([&] { x = 1; }));
  rc.NotifyCompletion(Status(rpc::STATUS_FAILED));
  ASSERT_EQ(1, x);

  rc.Reset();
  std::this_thread::sleep_for(100ms);
  EXPECT_NEAR(rc.GetElapsedTime() / 1ms, 100, 20);
}

TEST(RpcClientController, DefaultTimeout) {
  RpcClientController ctlr;
  ASSERT_NEAR((ctlr.GetTimeout() - ReadSteadyClock()) / 1ms, 2s / 1ms, 100);
  std::this_thread::sleep_for(3s);
  ctlr.Reset();
  ASSERT_NEAR((ctlr.GetTimeout() - ReadSteadyClock()) / 1ms, 2s / 1ms, 100);
}

class RpcClientControllerTest : public ::testing::Test {
 public:
  void SetUp() override {
    listening_on_ = testing::PickAvailableEndpoint();
    server_ = std::make_unique<Server>();
    server_->AddService(std::make_unique<ServiceImpl>());
    server_->AddProtocol("flare");
    server_->ListenOn(listening_on_);
    server_->Start();

    channel_ = std::make_unique<RpcChannel>();
    channel_->Open("flare://" + listening_on_.ToString());

    stub_ = std::make_unique<testing::EchoService_Stub>(channel_.get());
    sync_stub_ =
        std::make_unique<testing::EchoService_SyncStub>(channel_.get());
  }

  void TearDown() override {
    server_->Stop();
    server_->Join();
    server_.reset();
    channel_.reset();
    stub_.reset();
    sync_stub_.reset();
  }

 protected:
  Endpoint listening_on_;
  std::unique_ptr<Server> server_;
  std::unique_ptr<RpcChannel> channel_;
  std::unique_ptr<testing::EchoService_Stub> stub_;
  std::unique_ptr<testing::EchoService_SyncStub> sync_stub_;
};

TEST_F(RpcClientControllerTest, Timeout) {
  static const auto kBody = "test body"s;
  testing::EchoRequest req;
  testing::EchoResponse resp;

  RpcClientController ctlr;
  ctlr.SetTimeout(ReadSteadyClock() + 1000ms);
  req.set_body(std::to_string(500));  // Response delay, actually.
  stub_->Echo(&ctlr, &req, &resp, nullptr);
  ASSERT_FALSE(ctlr.Failed());
  ASSERT_EQ("500", resp.body());

  ctlr.Reset();
  ctlr.SetTimeout(ReadSteadyClock() + 1000ms);
  req.set_body(std::to_string(2000));
  stub_->Echo(&ctlr, &req, &resp, nullptr);
  ASSERT_TRUE(ctlr.Failed());
  ASSERT_EQ(rpc::STATUS_TIMEOUT, ctlr.ErrorCode());

  ctlr.Reset();
  ctlr.SetTimeout(ReadSteadyClock() + 1000ms);
  req.set_body(std::to_string(500));
  stub_->Echo(&ctlr, &req, &resp, nullptr);
  ASSERT_FALSE(ctlr.Failed());
  ASSERT_EQ("500", resp.body());

  ctlr.Reset();
  ctlr.SetTimeout(ReadSteadyClock() + 1000ms);
  req.set_body(std::to_string(2000));
  stub_->Echo(&ctlr, &req, &resp, nullptr);
  ASSERT_TRUE(ctlr.Failed());
}

TEST_F(RpcClientControllerTest, Timestamps) {
  static const auto kBody = "test body"s;

  auto start = ReadSteadyClock();
  testing::EchoRequest req;
  testing::EchoResponse resp;

  RpcClientController ctlr;
  req.set_body(std::to_string(50));  // Response delay, actually.
  stub_->Echo(&ctlr, &req, &resp, nullptr);
  ASSERT_FALSE(ctlr.Failed());
  ASSERT_EQ("50", resp.body());
  FLARE_LOG_INFO("Time elapsed: {}us , {}us, {}us, {}us.",
                 (ctlr.GetTimestampSent() - start) / 1us,
                 (ctlr.GetTimestampReceived() - start) / 1us,
                 (ctlr.GetTimestampParsed() - start) / 1us,
                 ctlr.GetElapsedTime() / 1us);
  // Well it's not very accurate so let's give an error tolerance. Otherwise the
  // assertion can fail if the machine is under heavy load.
  ASSERT_LE(start, ctlr.GetTimestampSent() + 5ms);
  ASSERT_LE(ctlr.GetTimestampSent(), ctlr.GetTimestampReceived());
  ASSERT_LE(ctlr.GetTimestampReceived(), ctlr.GetTimestampParsed());
  ASSERT_LE(ctlr.GetTimestampParsed(),
            ReadSteadyClock() + ctlr.GetElapsedTime());
}

TEST_F(RpcClientControllerTest, StreamDefaultTimeout) {
  testing::EchoRequest req;
  RpcClientController ctlr;
  auto start = ReadSteadyClock();
  auto reader = sync_stub_->EchoStreamResponse(req, &ctlr);
  ASSERT_FALSE(reader.Read());
  auto time_expired = ReadSteadyClock() - start;

  // We didn't set a timeout, but the default timeout (5s, we overrode it)
  // should apply.
  ASSERT_NEAR(time_expired / 1s, 5, 2);
}

TEST_F(RpcClientControllerTest, RawBytesInRequest) {
  static const auto kFancyBody = std::string(12345, 'a');
  testing::EchoRequest req;
  testing::EchoResponse resp;
  RpcClientController ctlr;
  req.set_body(kFancyBody);
  ctlr.SetTimeout(5s);
  ctlr.SetRequestRawBytes(CreateBufferSlow(req.SerializeAsString()));
  stub_->Echo(&ctlr, nullptr, &resp, nullptr);
  ASSERT_FALSE(ctlr.Failed());
  ASSERT_EQ(kFancyBody, resp.body());
}

TEST_F(RpcClientControllerTest, RawBytesInResponse) {
  static const auto kFancyBody = std::string(12345, 'a');
  testing::EchoRequest req;
  RpcClientController ctlr;
  req.set_body(kFancyBody);
  ctlr.SetTimeout(5s);
  ctlr.SetAcceptResponseRawBytes(true);
  auto result = sync_stub_->Echo(req, &ctlr);
  ASSERT_TRUE(result);

  testing::EchoResponse resp;
  ASSERT_TRUE(resp.ParseFromString(FlattenSlow(ctlr.GetResponseRawBytes())));
  ASSERT_EQ(kFancyBody, resp.body());
}

TEST_F(RpcClientControllerTest, RawBytesInRequestResponse) {
  static const auto kFancyBody = std::string(12345, 'a');
  testing::EchoRequest req;
  RpcClientController ctlr;
  req.set_body(kFancyBody);
  ctlr.SetTimeout(5s);
  ctlr.SetAcceptResponseRawBytes(true);
  ctlr.SetRequestRawBytes(CreateBufferSlow(req.SerializeAsString()));
  stub_->Echo(&ctlr, nullptr, nullptr, nullptr);
  ASSERT_FALSE(ctlr.Failed());

  testing::EchoResponse resp;
  ASSERT_TRUE(resp.ParseFromString(FlattenSlow(ctlr.GetResponseRawBytes())));
  ASSERT_EQ(kFancyBody, resp.body());
}

}  // namespace flare

FLARE_TEST_MAIN
