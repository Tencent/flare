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

#include "thirdparty/googletest/gtest/gtest.h"

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

constexpr auto kErrorCode = 12345;
constexpr auto kErrorDesc = "The streaming call failed.";

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

class EchoServiceError : public testing::SyncEchoService {
 public:
  void EchoStreamResponse(const testing::EchoRequest& request,
                          StreamWriter<testing::EchoResponse> writer,
                          RpcServerController* controller) override {
    controller->SetFailed(kErrorCode, kErrorDesc);
    writer.Close();
  }
};

}  // namespace

void CreateServerFrom(Server* server, google::protobuf::Service* service,
                      const Endpoint& ep) {
  server->AddProtocol("qzone-pb");
  server->AddService(MaybeOwning(non_owning, service));
  server->ListenOn(ep);
  server->Start();
}

testing::EchoService_SyncStub CreateStubTo(const Endpoint& ep) {
  auto channel = std::make_unique<RpcChannel>();
  CHECK(channel->Open(
      "qzone-pb://" + ep.ToString(),
      RpcChannel::Options{.maximum_packet_size = 64 * 1024 * 1024}));
  return testing::EchoService_SyncStub(std::move(channel));
}

TEST(QZoneProtocolStreamingRpc, BothSideFlare) {
  for (int k = 0; k != 20; ++k) {
    for (int j = 0; j != 10; ++j) {
      static const auto kHeavyPayloadSize = k * 1048576 * 2;
      EchoServiceFlare svc(kHeavyPayloadSize);
      auto ep = testing::PickAvailableEndpoint();
      Server server;
      CreateServerFrom(&server, &svc, ep);
      auto stub = CreateStubTo(ep);

      // RPC.
      RpcClientController ctlr;
      testing::EchoRequest request;
      request.set_body("hi there");
      auto is = stub.EchoStreamResponse(request, &ctlr);
      is.SetExpiration(ReadSteadyClock() + 10s);
      int i = 0;
      // Don't read until `is.Read()` fails as there's no end-of-stream marker
      // so the last read is guaranteed to timeout.
      while (i != 5) {
        auto rc = is.Read();
        ASSERT_TRUE(!!rc);
        EXPECT_EQ("hi there" + std::to_string(i++) +
                      std::string(kHeavyPayloadSize, 'a'),
                  rc->body());
      }
      is.Close();
      ASSERT_FALSE(ctlr.Failed());

      server.Stop();
      server.Join();
    }
  }
}

TEST(QZoneProtocolStreamingRpc, Timeout) {
  EchoServiceTimeout svc;
  auto ep = testing::PickAvailableEndpoint();
  Server server;
  CreateServerFrom(&server, &svc, ep);
  auto stub = CreateStubTo(ep);

  // RPC.
  RpcClientController ctlr;
  testing::EchoRequest request;
  request.set_body("hi there");
  auto is = stub.EchoStreamResponse(request, &ctlr);
  is.SetExpiration(ReadSteadyClock() + 1s);
  ASSERT_FALSE(is.Read());
  ASSERT_TRUE(ctlr.Failed());

  server.Stop();
  server.Join();
}

TEST(QZoneProtocolStreamingRpc, Error) {
  EchoServiceError svc;
  auto ep = testing::PickAvailableEndpoint();
  Server server;
  CreateServerFrom(&server, &svc, ep);
  auto stub = CreateStubTo(ep);

  // RPC.
  RpcClientController ctlr;
  testing::EchoRequest request;
  request.set_body("hi there");
  auto is = stub.EchoStreamResponse(request, &ctlr);
  is.SetExpiration(ReadSteadyClock() + 1s);
  ASSERT_FALSE(is.Read());
  ASSERT_TRUE(ctlr.Failed());
  ASSERT_EQ(kErrorCode, ctlr.ErrorCode());
  // QZone protocol does not support passing error text, no test for
  // `ctlr.ErrorText()` here.

  server.Stop();
  server.Join();
}

}  // namespace flare::protobuf

FLARE_TEST_MAIN
