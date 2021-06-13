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

#include "flare/rpc/protocol/http/builtin/rpc_statistics_handler.h"

#include <chrono>
#include <thread>

#include "gtest/gtest.h"
#include "jsoncpp/json.h"

#include "flare/base/down_cast.h"
#include "flare/net/http/http_client.h"
#include "flare/rpc/rpc_channel.h"
#include "flare/rpc/rpc_client_controller.h"
#include "flare/rpc/rpc_server_controller.h"
#include "flare/rpc/server.h"
#include "flare/testing/echo_service.flare.pb.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"

namespace flare::rpc::builtin {

namespace {

class Impl : public testing::SyncEchoService {
 public:
  void Echo(const testing::EchoRequest& request,
            testing::EchoResponse* response,
            RpcServerController* controller) override {
    response->set_body(request.body());
  }
};

}  // namespace

TEST(RpcStatisticsHandler, All) {
  auto ep = testing::PickAvailableEndpoint();
  Impl svc_impl;
  Server server;
  server.AddProtocols({"flare", "http", "http+pb"});
  server.AddService(MaybeOwning(non_owning, &svc_impl));
  server.ListenOn(ep);
  server.Start();

  RpcChannel channel;
  RpcClientController ctlr;
  CHECK(channel.Open("flare://" + ep.ToString(),
                     RpcChannel::Options{.override_nslb = "list+rr"}));
  testing::EchoService_SyncStub stub(&channel);
  testing::EchoRequest req;
  req.set_body("hello");
  ASSERT_EQ("hello", stub.Echo(req, &ctlr)->body());

  std::this_thread::sleep_for(std::chrono::seconds(2));

  HttpClient downloader;
  auto resp = downloader.Get("http://" + ep.ToString() + "/inspect/rpc_stats");
  ASSERT_TRUE(resp);
  Json::Value jsv;
  CHECK(Json::Reader().parse(*resp->body(), jsv));
  EXPECT_EQ(1, jsv["global"]["counter"]["success"]["total"].asUInt());

  resp = downloader.Get("http://" + ep.ToString() +
                        "/inspect/rpc_stats/global/counter");
  ASSERT_TRUE(resp);
  CHECK(Json::Reader().parse(*resp->body(), jsv));
  EXPECT_EQ(1, jsv["success"]["total"].asUInt());

  resp = downloader.Get("http://" + ep.ToString() + "/inspect/rpc_stats123");
  EXPECT_EQ(HttpStatus::NotFound, resp->status());

  server.Stop();
  server.Join();
}

}  // namespace flare::rpc::builtin

FLARE_TEST_MAIN
