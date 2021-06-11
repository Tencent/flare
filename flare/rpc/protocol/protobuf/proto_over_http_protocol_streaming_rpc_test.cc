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

#include <sys/signal.h>

#include "googletest/gtest/gtest.h"

#include "flare/base/callback.h"
#include "flare/base/random.h"
#include "flare/base/thread/latch.h"
#include "flare/fiber/this_fiber.h"
#include "flare/rpc/rpc_channel.h"
#include "flare/rpc/rpc_client_controller.h"
#include "flare/rpc/rpc_server_controller.h"
#include "flare/rpc/server.h"
#include "flare/testing/echo_service.flare.pb.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"

using namespace std::literals;

namespace flare::protobuf {

namespace {

class EchoServiceFlare : public testing::SyncEchoService {
 public:
  explicit EchoServiceFlare(std::size_t payload_size)
      : payload_size_(payload_size) {}

  void EchoStreamResponse(const testing::EchoRequest& request,
                          StreamWriter<testing::EchoResponse> writer,
                          RpcServerController* controller) override {
    for (int i = 0; i != 5; ++i) {
      testing::EchoResponse resp;
      resp.set_body(request.body() + std::to_string(i) +
                    std::string(payload_size_, 'a'));
      writer.Write(resp);
    }
    writer.Close();
  }

 private:
  std::size_t payload_size_;
};

class EchoServiceTimeout : public testing::SyncEchoService {
 public:
  void EchoStreamResponse(const testing::EchoRequest& request,
                          StreamWriter<testing::EchoResponse> writer,
                          RpcServerController* controller) override {
    this_fiber::SleepFor(2s);
    writer.Close();
  }
};

constexpr auto kErrorDesc = "ummm, you failed.";

class EchoServiceError : public testing::SyncEchoService {
 public:
  void EchoStreamResponse(const testing::EchoRequest& request,
                          StreamWriter<testing::EchoResponse> writer,
                          RpcServerController* controller) override {
    controller->SetFailed(rpc::STATUS_FROM_USER, kErrorDesc);
    writer.Close();
  }
};

}  // namespace

TEST(ProtoOverHttpProtocolStreamingRpc, BothSideFlare) {
  for (int k = 0; k != 100; ++k) {
    static const auto kHeavyPayloadSize = Random(16 * 1048576);
    EchoServiceFlare svc(kHeavyPayloadSize);
    Server server;
    auto ep = testing::PickAvailableEndpoint();

    server.AddProtocol("http+pb");
    server.AddService(MaybeOwning(non_owning, &svc));
    server.ListenOn(ep);
    server.Start();

    // Client side.
    RpcChannel channel;
    channel.Open("http+pb://" + ep.ToString(),
                 RpcChannel::Options{.maximum_packet_size = 64 * 1024 * 1024});
    testing::EchoService_SyncStub stub(&channel);
    RpcClientController ctlr;

    // RPC.
    testing::EchoRequest request;
    request.set_body("hi there");
    auto is = stub.EchoStreamResponse(request, &ctlr);
    int i = 0;
    is.SetExpiration(ReadSteadyClock() + 10s);
    while (auto rc = is.Read()) {
      EXPECT_EQ("hi there" + std::to_string(i++) +
                    std::string(kHeavyPayloadSize, 'a'),
                rc->body());
    }
    EXPECT_EQ(5, i);
    EXPECT_FALSE(ctlr.Failed());

    server.Stop();
    server.Join();
  }
}

TEST(ProtoOverHttpProtocolStreamingRpc, Timeout) {
  // Server side.
  EchoServiceTimeout svc;
  Server server;
  auto ep = testing::PickAvailableEndpoint();

  server.AddProtocol("http+pb");
  server.AddService(MaybeOwning(non_owning, &svc));
  server.ListenOn(ep);
  server.Start();

  // Client side.
  RpcChannel channel;
  channel.Open("http+pb://" + ep.ToString());
  testing::EchoService_SyncStub stub(&channel);
  RpcClientController ctlr;

  // RPC.
  testing::EchoRequest request;
  request.set_body("hi there");
  auto is = stub.EchoStreamResponse(request, &ctlr);
  is.SetExpiration(ReadSteadyClock() + 1s);
  ASSERT_FALSE(is.Read());
  ASSERT_TRUE(ctlr.Failed());

  server.Stop();
  server.Join();
}

TEST(ProtoOverHttpProtocolStreamingRpc, ServerSideError) {
  // Server side.
  EchoServiceError svc;
  Server server;
  auto ep = testing::PickAvailableEndpoint();

  server.AddProtocol("http+pb");
  server.AddService(MaybeOwning(non_owning, &svc));
  server.ListenOn(ep);
  server.Start();

  // Client side.
  RpcChannel channel;
  channel.Open("http+pb://" + ep.ToString());
  testing::EchoService_SyncStub stub(&channel);
  RpcClientController ctlr;

  // RPC.
  testing::EchoRequest request;
  request.set_body("hi there");
  auto is = stub.EchoStreamResponse(request, &ctlr);
  ASSERT_FALSE(is.Read());
  ASSERT_EQ(kErrorDesc, ctlr.ErrorText());
  ASSERT_TRUE(ctlr.Failed());

  server.Stop();
  server.Join();
}

}  // namespace flare::protobuf

FLARE_TEST_MAIN
