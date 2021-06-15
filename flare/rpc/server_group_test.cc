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

#include "flare/rpc/server_group.h"

#include <memory>
#include <utility>

#include "gtest/gtest.h"

#include "flare/net/http/http_client.h"
#include "flare/rpc/http_handler.h"
#include "flare/rpc/rpc_channel.h"
#include "flare/rpc/rpc_client_controller.h"
#include "flare/rpc/server.h"
#include "flare/testing/echo_service.flare.pb.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"

using namespace std::literals;

namespace flare {

class DummyService : public testing::SyncEchoService {
 public:
  void Echo(const testing::EchoRequest& request,
            testing::EchoResponse* response,
            RpcServerController* controller) override {
    response->set_body(request.body());
  }
};

class DummyPost : public HttpHandler {
 public:
  void OnPost(const HttpRequest& request, HttpResponse* response,
              HttpServerContext* context) override {
    response->set_body(std::move(*request.body()));
  }
};

TEST(ServerGroup, SingleServer) {
  auto ep = testing::PickAvailableEndpoint();

  ServerGroup svr_group;
  svr_group.AddServer(ep, {"flare"}, std::make_unique<DummyService>());
  svr_group.Start();

  RpcChannel channel;
  CHECK(channel.Open("flare://" + ep.ToString(),
                     RpcChannel::Options{.override_nslb = "list+rr"}));
  RpcClientController ctlr;
  testing::EchoService_SyncStub stub(&channel);
  testing::EchoRequest req;

  req.set_body("123");
  ASSERT_EQ("123", stub.Echo(req, &ctlr)->body());

  svr_group.Stop();
  svr_group.Join();
}

TEST(ServerGroup, MultipleServer) {
  auto ep1 = testing::PickAvailableEndpoint();
  auto ep2 = testing::PickAvailableEndpoint();

  ServerGroup svr_group;
  svr_group.AddServer(ep1, {"flare"}, std::make_unique<DummyService>());
  auto http_svr = svr_group.AddServer();
  http_svr->ListenOn(ep2);
  http_svr->AddHttpHandler("/path/to/post", std::make_unique<DummyPost>());
  http_svr->AddProtocol("http");
  svr_group.Start();

  // Test server 1.
  RpcChannel channel;
  CHECK(channel.Open("flare://" + ep1.ToString(),
                     RpcChannel::Options{.override_nslb = "list+rr"}));
  RpcClientController ctlr;
  testing::EchoService_SyncStub stub(&channel);
  testing::EchoRequest req;
  req.set_body("123");
  ASSERT_EQ("123", stub.Echo(req, &ctlr)->body());

  // Test server 2.

  HttpClient client;
  HttpClient::RequestOptions opts;
  opts.content_type = "application/text";
  auto resp =
      client.Post("http://" + ep2.ToString() + "/path/to/post", "abc", opts);
  ASSERT_TRUE(resp);
  ASSERT_EQ("abc", *resp->body());

  svr_group.Stop();
  svr_group.Join();
}

TEST(ServerGroup, AutoStopAndJoin) {
  auto ep = testing::PickAvailableEndpoint();

  ServerGroup svr_group;
  svr_group.AddServer(ep, {"flare"}, std::make_unique<DummyService>());
  svr_group.Start();

  // Implicitly stopped & joined.
}

}  // namespace flare

FLARE_TEST_MAIN
