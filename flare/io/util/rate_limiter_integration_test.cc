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

#include "flare/fiber/async.h"
#include "flare/init/override_flag.h"
#include "flare/rpc/rpc_channel.h"
#include "flare/rpc/rpc_client_controller.h"
#include "flare/rpc/rpc_server_controller.h"
#include "flare/rpc/server.h"
#include "flare/testing/echo_service.flare.pb.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"

using namespace std::literals;

FLARE_OVERRIDE_FLAG(flare_io_cap_rx_bandwidth, "1M");
FLARE_OVERRIDE_FLAG(flare_io_cap_tx_bandwidth, "1M");

namespace flare {

namespace {

class DummyEcho : public testing::SyncEchoService {
 public:
  void Echo(const testing::EchoRequest& request,
            testing::EchoResponse* response,
            RpcServerController* controller) override {
    response->set_body(request.body());
  }
};

}  // namespace

TEST(RateLimiterIntegrationTest, All) {
  constexpr auto kBodySize = 1 * 1024 * 1024 / 8;  // 1Mbit.
  static const std::string kBody(kBodySize, 'A');
  constexpr auto kRequests = 10;

  auto listening_ep = testing::PickAvailableEndpoint();
  Server server(
      // Slightly greater than 4M should be enough. (4M body + protocol
      // overhead)
      Server::Options{.maximum_packet_size = 64 * 1024 * 1024});

  server.AddService(std::make_unique<DummyEcho>());
  server.AddProtocol("flare");
  server.ListenOn(listening_ep);
  server.Start();

  std::vector<Future<>> ops;
  for (int i = 0; i != kRequests; ++i) {
    ops.push_back(fiber::Async([&listening_ep] {
      RpcChannel channel;
      channel.Open("flare://" + listening_ep.ToString(),
                   RpcChannel::Options{.maximum_packet_size = 64 * 1024 * 1024,
                                       .override_nslb = "list+rr"});
      testing::EchoService_SyncStub stub(&channel);
      testing::EchoRequest req;
      RpcClientController ctlr;
      ctlr.SetTimeout(ReadSteadyClock() + 100s);
      req.set_body(kBody);
      ASSERT_EQ(kBody, stub.Echo(req, &ctlr)->body());
    }));
  }

  auto start = ReadSteadyClock();
  fiber::BlockingGet(WhenAll(&ops));
  auto time_elapsed = ReadSteadyClock() - start;

  // Each request uses 2 sec. (1 sec for send, 1 sec for receive.)
  ASSERT_NEAR(time_elapsed / 1ms, kRequests * 2s / 1ms, 1s / 1ms);

  server.Stop();
  server.Join();
}

}  // namespace flare

FLARE_TEST_MAIN
