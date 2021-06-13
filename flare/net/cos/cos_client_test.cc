// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "flare/net/cos/cos_client.h"

#include <chrono>
#include <string>

#include "gtest/gtest.h"

#include "flare/base/buffer.h"
#include "flare/base/enum.h"
#include "flare/fiber/this_fiber.h"
#include "flare/net/cos/cos_status.h"
#include "flare/net/cos/ops/operation.h"
#include "flare/rpc/http_handler.h"
#include "flare/rpc/server.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"

using namespace std::literals;

namespace flare {

namespace {

auto listening_ep = flare::testing::PickAvailableEndpoint();

class TestingOperation : public cos::CosOperation {
 public:
  bool PrepareTask(cos::CosTask* task, ErasedPtr* context) const override {
    EXPECT_EQ("my-region", task->options().region);
    EXPECT_EQ("bucket", task->options().bucket);
    EXPECT_EQ("sid", task->options().secret_id);
    EXPECT_EQ("skey", task->options().secret_key);
    task->set_method(HttpMethod::Post);
    task->set_uri(
        Format("http://{}/{}", listening_ep.ToString(), expected_path));
    task->AddHeader("some-fancy-header:and-its-value");
    task->set_body("bodybodybody");
    return true;
  }

  std::string expected_path;
};

class TestingOperationResult : public cos::CosOperationResult {
 public:
  bool ParseResult(cos::CosTaskCompletion completion,
                   ErasedPtr context) override {
    resp_body = FlattenSlow(*completion.body());
    return true;
  }

  std::string resp_body;
};

}  // namespace

namespace cos {

template <>
struct cos_result<TestingOperation> {
  using type = TestingOperationResult;
};

}  // namespace cos

// This UT only tests if `CosClient` can perform actions correctly, yet it does
// not test if individual actions' implementation.
//
// Individual actions are tested in their respective UT in `ops/`.
TEST(CosClient, Basic) {
  Server server;
  server.AddProtocol("http");
  server.AddHttpHandler(
      "/cos-test", NewHttpPostHandler([](auto&& req, auto&& resp, auto&& ctx) {
        EXPECT_EQ("and-its-value", req.headers()->TryGet("some-fancy-header"));
        EXPECT_EQ("bodybodybody", *req.body());
        for (auto&& [k, v] : *req.headers()) {
          FLARE_LOG_INFO("Received: {} -> {}", k, v);
        }
        resp->set_body("an empty body");
      }));
  server.AddHttpHandler(
      "/timeout", NewHttpPostHandler([](auto&& req, auto&& resp, auto&& ctx) {
        this_fiber::SleepFor(2s);
      }));
  server.ListenOn(listening_ep);
  server.Start();

  CosClient client;
  ASSERT_TRUE(client.Open(
      "cos://my-region",
      CosClient::Options{
          .secret_id = "sid", .secret_key = "skey", .bucket = "bucket"}));

  TestingOperation op;
  op.expected_path = "cos-test";
  auto result = client.Execute(op);
  ASSERT_TRUE(result) << result.error().ToString();
  EXPECT_EQ("an empty body", result->resp_body);

  op.expected_path = "timeout";
  result = client.Execute(op, 1s);
  ASSERT_FALSE(result);
  EXPECT_EQ(underlying_value(CosStatus::Timeout), result.error().code());

  op.expected_path = "404";
  result = client.Execute(op);
  ASSERT_FALSE(result);
  EXPECT_EQ(underlying_value(CosStatus::MalformedResponse),
            result.error().code());
}

}  // namespace flare

FLARE_TEST_MAIN
