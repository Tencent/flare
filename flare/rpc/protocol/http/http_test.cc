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

#include <chrono>

#include "googletest/gtest/gtest.h"

#include "flare/base/string.h"
#include "flare/net/http/http_client.h"
#include "flare/rpc/http_filter.h"
#include "flare/rpc/http_handler.h"
#include "flare/rpc/server.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"

using namespace std::literals;

namespace flare {

class HttpTest : public ::testing::Test {
 public:
  void SetUp() override {
    server_ = std::make_unique<Server>();
    server_->AddProtocol("http");
    server_->ListenOn(listening_ep_);
  }
  void TearDown() override { server_ = nullptr; }

 protected:
  Endpoint listening_ep_ = testing::PickAvailableEndpoint();
  std::unique_ptr<Server> server_;
};

class MyFilter : public HttpFilter {
 public:
  Action OnFilter(HttpRequest* request, HttpResponse* response,
                  HttpServerContext* context) override {
    if (*request->body() == "filter-drop") {
      return Action::Drop;
    } else if (*request->body() == "filter-early-return") {
      response->set_body("from filter");
      return Action::EarlyReturn;
    }
    return Action::KeepProcessing;
  }
};

auto HttpPost(const std::string& uri, const std::string& body) {
  HttpClient client;
  return client.Post(uri, body, {});
}

TEST_F(HttpTest, Filter) {
  server_->AddHttpFilter(std::make_unique<MyFilter>());
  server_->AddHttpHandler(
      "/test", NewHttpPostHandler([&](auto&& req, auto&& resp, auto&& ctx) {
        resp->set_body(*req.body());
      }));
  server_->Start();

  auto uri = Format("http://{}/test", listening_ep_.ToString());
  ASSERT_FALSE(HttpPost(uri, "filter-drop"));
  ASSERT_EQ("something else", *HttpPost(uri, "something else")->body());
  ASSERT_EQ("from filter", *HttpPost(uri, "filter-early-return")->body());
}

TEST_F(HttpTest, 404) {
  server_->Start();
  auto uri = Format("http://{}/test", listening_ep_.ToString());
  ASSERT_EQ(HttpStatus::NotFound, HttpPost(uri, "filter-drop")->status());
}

TEST_F(HttpTest, DefaultHandler) {
  server_->SetDefaultHttpHandler(
      NewHttpHandler([](auto&& req, auto&& resp, auto&& ctx) {
        resp->set_body(*req.body());
      }));
  server_->Start();
  auto uri = Format("http://{}/404-path", listening_ep_.ToString());
  ASSERT_EQ("something", *HttpPost(uri, "something")->body());
}

TEST_F(HttpTest, UriWithQuery) {
  server_->AddHttpHandler(
      "/test", NewHttpHandler([](auto&& req, auto&& resp, auto&& ctx) {
        resp->set_body(*req.body());
      }));
  server_->Start();
  auto uri = Format("http://{}/test?a=1", listening_ep_.ToString());
  ASSERT_EQ("something", *HttpPost(uri, "something")->body());
}

TEST_F(HttpTest, ShortConnection) {
  server_->AddHttpHandler(
      "/test", NewHttpHandler([](auto&& req, auto&& resp, auto&& ctx) {
        resp->set_body(*req.body());
      }));
  server_->Start();
  auto uri = Format("http://{}/test?a=1", listening_ep_.ToString());

  HttpClient client;
  auto result =
      client.Post(uri, "something",
                  HttpClient::RequestOptions{.headers = {"Connection: close"}});
  ASSERT_EQ("something", *result->body());
}

}  // namespace flare

FLARE_TEST_MAIN
