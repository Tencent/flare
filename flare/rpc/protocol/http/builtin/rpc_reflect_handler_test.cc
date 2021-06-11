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

#include "flare/rpc/protocol/http/builtin/rpc_reflect_handler.h"

#include <memory>
#include <string>

#include "googletest/gtest/gtest.h"
#include "jsoncpp/reader.h"
#include "protobuf/descriptor.pb.h"

#include "flare/base/net/endpoint.h"
#include "flare/net/http/http_client.h"
#include "flare/rpc/protocol/protobuf/service_method_locator.h"
#include "flare/rpc/server.h"
#include "flare/testing/echo_service.flare.pb.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"

namespace flare::rpc::builtin {

struct TestEchoService : testing::EchoService {};

class RpcBuiltinServiceTest : public ::testing::Test {
 protected:
  RpcBuiltinServiceTest() {}
  void SetUp() override {
    ep_ = testing::PickAvailableEndpoint();
    server_.AddService(std::make_unique<TestEchoService>());
    server_.ListenOn(ep_);
    server_.Start();
  }

  void TearDown() override {
    server_.Stop();
    server_.Join();
  }

  Server server_;
  Endpoint ep_;
  RpcReflectHandler handler_;
  HttpClient client_;
};

TEST_F(RpcBuiltinServiceTest, GetService) {
  auto&& response =
      client_.Get("http://" + ep_.ToString() + "/inspect/rpc_reflect/services");
  ASSERT_TRUE(response);
  ASSERT_EQ(HttpStatus::OK, response->status());
  Json::Value root;
  std::string body = *response->body();
  Json::Reader reader;
  EXPECT_TRUE(reader.parse(body.data(), body.data() + body.size(), root));
  EXPECT_EQ(1, root["service"].size());
  EXPECT_EQ("flare.testing.EchoService",
            root["service"][0]["full_name"].asString());
}

TEST_F(RpcBuiltinServiceTest, GetMethod) {
  auto&& response =
      client_.Get("http://" + ep_.ToString() +
                  "/inspect/rpc_reflect/method/flare.testing.EchoService.Echo");
  ASSERT_TRUE(response);

  ASSERT_EQ(HttpStatus::OK, response->status());
  Json::Value root;
  std::string body = *response->body();

  Json::Reader reader;
  EXPECT_TRUE(reader.parse(body.data(), body.data() + body.size(), root));

  EXPECT_EQ("Echo", root["method"]["name"].asString());
  EXPECT_GE(1, root["message_type"].size());
}

}  // namespace flare::rpc::builtin

FLARE_TEST_MAIN
