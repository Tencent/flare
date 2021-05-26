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

#include "flare/testing/rpc_mock.h"

#include "flare/rpc/rpc_channel.h"
#include "flare/rpc/rpc_client_controller.h"
#include "flare/testing/echo_service.flare.pb.h"
#include "flare/testing/main.h"

using namespace std::literals;

using testing::_;

namespace flare::testing::detail {

TEST(RpcMock, HandleRpc) {
  constexpr auto kResponse = "mock resp";
  RpcChannel channel;
  FLARE_CHECK(channel.Open("mock://..."));
  EchoService_SyncStub ss(&channel);

  auto handler = [&](const EchoRequest& req, EchoResponse* resp,
                     RpcServerController* ctlr) {
    resp->set_body(req.body() + ": 123");
  };

  EchoResponse er;
  er.set_body(kResponse);
  FLARE_EXPECT_RPC(EchoService::Echo, _)
      .WillRepeatedly(testing::HandleRpc(handler));

  EchoRequest req;
  RpcClientController ctlr;
  req.set_body("hi there");
  auto rc = ss.Echo(req, &ctlr);
  ASSERT_TRUE(rc);
  ASSERT_EQ("hi there: 123", rc->body());
}

TEST(RpcMock, NormalRpc) {
  constexpr auto kResponse = "mock resp";
  RpcChannel channel;
  FLARE_CHECK(channel.Open("mock://..."));
  EchoService_SyncStub ss(&channel);

  EchoResponse er;
  er.set_body(kResponse);
  FLARE_EXPECT_RPC(EchoService::Echo, _).WillRepeatedly(testing::Return(er));

  EchoRequest req;
  RpcClientController ctlr;
  auto rc = ss.Echo(req, &ctlr);
  ASSERT_TRUE(rc);
  ASSERT_EQ(kResponse, rc->body());
}

TEST(RpcMock, Fail) {
  constexpr auto kResponse = "mock resp";
  RpcChannel channel;
  FLARE_CHECK(channel.Open("mock://..."));
  EchoService_SyncStub ss(&channel);

  EchoResponse er;
  er.set_body(kResponse);
  FLARE_EXPECT_RPC(EchoService::Echo, _)
      .WillRepeatedly(testing::Return(rpc::STATUS_OVERLOADED, "overloaded"));

  EchoRequest req;
  RpcClientController ctlr;
  auto rc = ss.Echo(req, &ctlr);
  ASSERT_FALSE(rc);
  EXPECT_EQ(rpc::STATUS_OVERLOADED, rc.error().code());
  EXPECT_EQ("overloaded", rc.error().message());
}

TEST(RpcMock, DeprecatedNormalRpc) {
  constexpr auto kResponse = "mock resp";
  RpcChannel channel;
  FLARE_CHECK(channel.Open("mock://..."));
  EchoService_SyncStub ss(&channel);

  EchoResponse er;
  er.set_body(kResponse);
  FLARE_EXPECT_RPC(EchoService::Echo, _).WillRepeatedly(Respond(er));

  EchoRequest req;
  RpcClientController ctlr;
  auto rc = ss.Echo(req, &ctlr);
  ASSERT_TRUE(rc);
  ASSERT_EQ(kResponse, rc->body());
}

TEST(RpcMock, DeprecatedNormalRpcFail) {
  constexpr auto kResponse = "mock resp";
  RpcChannel channel;
  FLARE_CHECK(channel.Open("mock://..."));
  EchoService_SyncStub ss(&channel);

  EchoResponse er;
  er.set_body(kResponse);
  FLARE_EXPECT_RPC(EchoService::Echo, _)
      .WillRepeatedly(FailWith(rpc::STATUS_OVERLOADED));

  EchoRequest req;
  RpcClientController ctlr;
  auto rc = ss.Echo(req, &ctlr);
  ASSERT_FALSE(rc);
  ASSERT_EQ(rpc::STATUS_OVERLOADED, rc.error().code());
}

TEST(RpcMock, DeprecatedNormalRpcFail2) {
  constexpr auto kResponse = "mock resp";
  RpcChannel channel;
  FLARE_CHECK(channel.Open("mock://..."));
  EchoService_SyncStub ss(&channel);

  EchoResponse er;
  er.set_body(kResponse);
  FLARE_EXPECT_RPC(EchoService::Echo, _)
      .WillRepeatedly(FailWith(rpc::STATUS_FAILED, "Hello world"));

  EchoRequest req;
  RpcClientController ctlr;
  auto rc = ss.Echo(req, &ctlr);
  ASSERT_FALSE(rc);
  ASSERT_EQ(rpc::STATUS_FAILED, rc.error().code());
  ASSERT_EQ("Hello world", rc.error().message());
  ASSERT_EQ("Hello world", ctlr.ErrorText());
}

}  // namespace flare::testing::detail

FLARE_TEST_MAIN
