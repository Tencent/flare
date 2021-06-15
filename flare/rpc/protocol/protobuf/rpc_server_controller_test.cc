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

#define FLARE_RPC_SERVER_CONTROLLER_SUPPRESS_INCLUDE_WARNING

#include "flare/rpc/protocol/protobuf/rpc_server_controller.h"

#include <sys/signal.h>

#include <chrono>
#include <thread>

#include "gtest/gtest.h"

#include "flare/base/logging.h"
#include "flare/fiber/this_fiber.h"
#include "flare/rpc/rpc_channel.h"
#include "flare/rpc/rpc_client_controller.h"
#include "flare/rpc/server.h"
#include "flare/testing/echo_service.flare.pb.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"

using namespace std::literals;

namespace flare {

class DummyEcho : public testing::SyncEchoService {
 public:
  void Echo(const testing::EchoRequest& request,
            testing::EchoResponse* response,
            RpcServerController* controller) override {
    if (test_cb_) {
      test_cb_(request, response, controller);
    }

    auto now = ReadSteadyClock();
    if (write_response_in_bytes_) {
      testing::EchoResponse alternative_resp;
      alternative_resp.set_body(request.body());

      controller->SetResponseRawBytes(
          CreateBufferSlow(alternative_resp.SerializeAsString()));
    } else {
      response->set_body(request.body());
    }
    // The first two will be negative.
    FLARE_LOG_INFO("Timestamps: {}us, {}us, {}us, {}us.",
                   (controller->GetTimestampReceived() - now) / 1us,
                   (controller->GetTimestampDispatched() - now) / 1us,
                   (controller->GetTimestampParsed() - now) / 1us,
                   controller->GetElapsedTime() / 1us);
    ASSERT_LE(controller->GetTimestampReceived(),
              controller->GetTimestampDispatched());
    ASSERT_LE(controller->GetTimestampDispatched(),
              controller->GetTimestampParsed());
    ASSERT_LE(controller->GetTimestampParsed(), now);
    ASSERT_LE(now, ReadSteadyClock() + controller->GetElapsedTime());
    controller->WriteResponseImmediately();

    this_fiber::SleepFor(1s);
  }
  std::atomic<bool> write_response_in_bytes_{false};
  Function<void(const testing::EchoRequest&, testing::EchoResponse*,
                RpcServerController*)>
      test_cb_;
} dummy;

TEST(RpcServerController, Basics) {
  RpcServerController rc;

  rc.SetFailed("failed");
  ASSERT_TRUE(rc.Failed());
  ASSERT_EQ("failed", rc.ErrorText());
}

TEST(RpcServerController, Timeout) {
  RpcServerController ctlr;
  ASSERT_FALSE(ctlr.GetTimeout());
  ctlr.SetTimeout(ReadSteadyClock() + 1s);
  ASSERT_TRUE(ctlr.GetTimeout());
  EXPECT_NEAR((*ctlr.GetTimeout() - ReadSteadyClock()) / 1ms, 1s / 1ms, 100);
  ctlr.Reset();
  ASSERT_FALSE(ctlr.GetTimeout());
}

TEST(RpcServerController, Compression) {
  RpcServerController ctlr;
  ctlr.SetAcceptableCompressionAlgorithm(
      (1 << rpc::COMPRESSION_ALGORITHM_SNAPPY) |
      (1 << rpc::COMPRESSION_ALGORITHM_ZSTD));
  EXPECT_FALSE(ctlr.GetAcceptableCompressionAlgorithms()
                   [rpc::COMPRESSION_ALGORITHM_LZ4_FRAME]);
  EXPECT_TRUE(ctlr.GetAcceptableCompressionAlgorithms()
                  [rpc::COMPRESSION_ALGORITHM_ZSTD]);
  // Acceptable even if not enabled explicitly.
  EXPECT_TRUE(ctlr.GetAcceptableCompressionAlgorithms()
                  [rpc::COMPRESSION_ALGORITHM_NONE]);
  EXPECT_EQ(rpc::COMPRESSION_ALGORITHM_ZSTD,
            ctlr.GetPreferredCompressionAlgorithm());
  EXPECT_EQ(rpc::COMPRESSION_ALGORITHM_NONE, ctlr.GetCompressionAlgorithm());
  ctlr.SetCompressionAlgorithm(rpc::COMPRESSION_ALGORITHM_ZSTD);
  EXPECT_EQ(rpc::COMPRESSION_ALGORITHM_ZSTD, ctlr.GetCompressionAlgorithm());
}

class RpcServerControllerTest : public ::testing::Test {
  void SetUp() override {
    listening_on_ = testing::PickAvailableEndpoint();
    server_ = std::make_unique<Server>();
    server_->AddService(&dummy);
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
    dummy.write_response_in_bytes_ = false;  // Always reset.
  }

 protected:
  Endpoint listening_on_;
  std::unique_ptr<Server> server_;
  std::unique_ptr<RpcChannel> channel_;
  std::unique_ptr<testing::EchoService_Stub> stub_;
  std::unique_ptr<testing::EchoService_SyncStub> sync_stub_;
};

TEST_F(RpcServerControllerTest, Timestamps) {
  testing::EchoRequest req;

  RpcClientController ctlr;
  req.set_body("aaa");
  ASSERT_EQ("aaa", sync_stub_->Echo(req, &ctlr)->body());
}

// For unimplemented method, the default implementation should fail the request
// gracefully.
TEST_F(RpcServerControllerTest, NotImplementedStreamingRequest) {
  testing::EchoRequest req;
  req.set_body("aaa");

  RpcClientController ctlr;
  auto [is, os] = sync_stub_->EchoStreamRequest(&ctlr);
  os.WriteLast(req);
  ASSERT_FALSE(is.Read());
  ASSERT_TRUE(ctlr.Failed());
  ASSERT_EQ(rpc::STATUS_FAILED, ctlr.ErrorCode());
}

TEST_F(RpcServerControllerTest, NotImplementedStreamingResponse) {
  testing::EchoRequest req;
  req.set_body("aaa");

  RpcClientController ctlr;
  ASSERT_FALSE(sync_stub_->EchoStreamResponse(req, &ctlr).Read());
  ASSERT_TRUE(ctlr.Failed());
  ASSERT_EQ(rpc::STATUS_FAILED, ctlr.ErrorCode());
}

TEST_F(RpcServerControllerTest, NotImplementedStreamingBoth) {
  testing::EchoRequest req;
  req.set_body("aaa");

  RpcClientController ctlr;
  auto [is, os] = sync_stub_->EchoStreamRequest(&ctlr);
  os.WriteLast(req);
  ASSERT_FALSE(is.Read());
  ASSERT_TRUE(ctlr.Failed());
  ASSERT_EQ(rpc::STATUS_FAILED, ctlr.ErrorCode());
}

// Not implemented yet.
//
// TEST_F(RpcServerControllerTest, BytesInRequest) {}

TEST_F(RpcServerControllerTest, ResponseRawBytes) {
  dummy.write_response_in_bytes_ = true;
  testing::EchoRequest req;

  RpcClientController ctlr;
  req.set_body("aaa");
  ASSERT_EQ("aaa", sync_stub_->Echo(req, &ctlr)->body());
}

TEST_F(RpcServerControllerTest, TimeoutFromClient) {
  dummy.write_response_in_bytes_ = true;
  testing::EchoRequest req;

  // No timeout is provided. The default 2s timeout is applied.
  dummy.test_cb_ = [&](auto&&, auto&&, RpcServerController* ctlr) {
    ASSERT_TRUE(ctlr->GetTimeout());
    EXPECT_NEAR((*ctlr->GetTimeout() - ReadSteadyClock()) / 1ms, 2s / 1ms, 100);
  };

  RpcClientController ctlr;
  req.set_body("aaa");
  ASSERT_EQ("aaa", sync_stub_->Echo(req, &ctlr)->body());

  ctlr.Reset();
  ctlr.SetTimeout(1s);
  dummy.test_cb_ = [&](auto&&, auto&&, RpcServerController* ctlr) {
    ASSERT_TRUE(ctlr->GetTimeout());
    EXPECT_NEAR((*ctlr->GetTimeout() - ReadSteadyClock()) / 1ms, 1s / 1ms, 100);
  };
  ASSERT_EQ("aaa", sync_stub_->Echo(req, &ctlr)->body());
}

}  // namespace flare

FLARE_TEST_MAIN
