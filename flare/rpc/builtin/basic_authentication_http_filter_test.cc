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

#include "flare/rpc/builtin/basic_authentication_http_filter.h"

#include <memory>
#include <string>
#include <vector>

#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/base/string.h"
#include "flare/net/http/http_client.h"
#include "flare/rpc/http_handler.h"
#include "flare/rpc/server.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"

namespace flare {

namespace {

auto HttpGet(const std::string& uri, const std::vector<std::string>& headers) {
  HttpClient client;
  return client.Get(uri, HttpClient::RequestOptions{.headers = headers});
}

auto MakeHttpHandler() {
  return NewHttpGetHandler(
      [](auto&&, auto&& resp, auto&&) { resp->set_body("handled."); });
}

}  // namespace

TEST(BasicAuthenticationHttpFilter, Integrated) {
  auto listening_ep = testing::PickAvailableEndpoint();

  Server server;
  server.AddProtocol("http");
  server.AddHttpFilter(std::make_unique<BasicAuthenticationHttpFilter>(
      [](auto&& user, auto&& pass) { return user == "Alice" && pass == "Bob"; },
      "/blocked"));
  server.AddHttpHandler("/blocked", MakeHttpHandler());
  server.AddHttpHandler("/blocked/subdir", MakeHttpHandler());
  server.AddHttpHandler("/free", MakeHttpHandler());
  server.ListenOn(listening_ep);
  server.Start();

  EXPECT_EQ(
      "handled.",
      *HttpGet(Format("http://{}/free", listening_ep.ToString()), {})->body());

  EXPECT_EQ(HttpStatus::Unauthorized,
            HttpGet(Format("http://{}/blocked", listening_ep.ToString()), {})
                ->status());
  EXPECT_EQ(
      HttpStatus::Unauthorized,
      HttpGet(Format("http://{}/blocked/subdir", listening_ep.ToString()), {})
          ->status());
  // Invalid credential.
  EXPECT_EQ(HttpStatus::Unauthorized,
            HttpGet(Format("http://{}/blocked", listening_ep.ToString()),
                    {"Authorization: Basic QWxhZGRpbjpPcGVuU2VzYW1l"})
                ->status());
  // Invalid base 64.
  EXPECT_EQ(HttpStatus::Unauthorized,
            HttpGet(Format("http://{}/blocked", listening_ep.ToString()),
                    {"Authorization: Basic QWxpY2U6Qm9"})
                ->status());
  EXPECT_EQ("handled.",
            *HttpGet(Format("http://{}/blocked", listening_ep.ToString()),
                     {"Authorization: Basic QWxpY2U6Qm9i"})
                 ->body());
}

}  // namespace flare

FLARE_TEST_MAIN
