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

#include "flare/net/internal/http_engine.h"

#include <chrono>
#include <string>
#include <vector>

#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/base/string.h"
#include "flare/fiber/latch.h"
#include "flare/fiber/this_fiber.h"
#include "flare/rpc/http_handler.h"
#include "flare/rpc/server.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"

using namespace std::literals;

namespace flare::internal {

// Non-conformant: Case sensitive. Won't hurt for UT, though.
std::string GetHeader(const std::vector<std::string>& headers,
                      const std::string& key) {
  for (auto&& e : headers) {
    if (StartsWith(e, key + ":")) {
      return std::string(Trim(e.substr(key.size() + 1)));
    }
  }
  return "";
}

class HttpEngineTest : public ::testing::Test {
 public:
  void SetUp() override {
    server_.AddProtocol("http");
    server_.AddHttpHandler(
        "/basic", flare::NewHttpGetHandler([](auto&& r, auto&& w, auto&& c) {
          w->set_body("hello world");
          w->headers()->Append("custom_header", "header_value");
          w->set_status(flare::HttpStatus::OK);
        }));
    server_.AddHttpHandler(
        "/timeout", flare::NewHttpGetHandler([](auto&& r, auto&& w, auto&& c) {
          this_fiber::SleepFor(200ms);
          w->set_status(flare::HttpStatus::OK);
        }));
    server_.AddHttpHandler(
        "/post", flare::NewHttpPostHandler([](auto&& r, auto&& w, auto&& c) {
          if (*r.body() == "local buffer") {
            w->set_status(flare::HttpStatus::OK);
          } else {
            w->set_status(flare::HttpStatus::BadRequest);
          }
        }));
    server_.AddHttpHandler(
        "/put", flare::NewHttpPutHandler([](auto&& r, auto&& w, auto&& c) {
          if (*r.body() == "local buffer") {
            w->set_status(flare::HttpStatus::OK);
          } else {
            w->set_status(flare::HttpStatus::BadRequest);
          }
        }));

    auto&& endpoint = testing::PickAvailableEndpoint();
    server_.ListenOn(endpoint);
    site_url_ = "http://" + endpoint.ToString() + "/";
    FLARE_CHECK(server_.Start());
    auto endpoint_str = endpoint.ToString();
    port_ =
        *TryParse<int>(endpoint_str.substr(endpoint_str.find_last_of(":") + 1));
  }

  Server server_;
  std::string site_url_;
  int port_;
};

TEST_F(HttpEngineTest, Basic) {
  HttpTask t;
  t.SetMethod(HttpMethod::Get);
  t.SetUrl(site_url_ + "basic");
  t.SetTimeout(1s);
  fiber::Latch l(1);
  HttpEngine::Instance()->StartTask(
      std::move(t), [&](Expected<HttpTaskCompletion, Status> resp) {
        EXPECT_TRUE(resp);
        EXPECT_EQ("hello world", FlattenSlow(*resp->body()));
        EXPECT_EQ("header_value", GetHeader(*resp->headers(), "custom_header"));
        EXPECT_EQ(HttpStatus::OK, resp->status());
        EXPECT_EQ(HttpVersion::V_1_1, resp->version());
        l.count_down();
      });
  l.wait();
}

TEST_F(HttpEngineTest, Put) {
  HttpTask t;
  t.SetMethod(HttpMethod::Put);
  t.SetUrl(site_url_ + "put");
  t.SetTimeout(1s);
  t.SetBody("local buffer");
  t.AddHeader("Expect:");  // No `100-continue`.
  fiber::Latch l(1);
  HttpEngine::Instance()->StartTask(
      std::move(t), [&](Expected<HttpTaskCompletion, Status> resp) {
        EXPECT_TRUE(resp);
        EXPECT_EQ(HttpStatus::OK, resp->status());
        l.count_down();
      });
  l.wait();
}

TEST_F(HttpEngineTest, SetBodyString) {
  HttpTask t;
  t.SetMethod(HttpMethod::Post);
  t.SetUrl(site_url_ + "post");
  t.SetTimeout(1s);
  {
    char local_buffer[1024] = "local buffer";
    t.SetBody(local_buffer);
  }
  fiber::Latch l(1);
  HttpEngine::Instance()->StartTask(
      std::move(t), [&](Expected<HttpTaskCompletion, Status> resp) {
        EXPECT_TRUE(resp);
        EXPECT_EQ(HttpStatus::OK, resp->status());
        l.count_down();
      });
  l.wait();
}

TEST_F(HttpEngineTest, SetBodyBuffer) {
  HttpTask t;
  t.SetMethod(HttpMethod::Post);
  t.SetUrl(site_url_ + "post");
  t.SetTimeout(1s);
  {
    std::string local = "local";
    NoncontiguousBufferBuilder builder;
    builder.Append(local);
    builder.Append(MakeForeignBuffer(" "));
    builder.Append(MakeForeignBuffer("buffer"));
    t.SetBody(builder.DestructiveGet());
  }
  fiber::Latch l(1);
  HttpEngine::Instance()->StartTask(
      std::move(t), [&](Expected<HttpTaskCompletion, Status> resp) {
        EXPECT_TRUE(resp);
        EXPECT_EQ(HttpStatus::OK, resp->status());
        l.count_down();
      });
  l.wait();
}

TEST_F(HttpEngineTest, Timeout) {
  HttpTask t;
  t.SetMethod(HttpMethod::Get);
  t.SetUrl(site_url_ + "timeout");
  t.SetTimeout(10ms);
  fiber::Latch l(1);
  HttpEngine::Instance()->StartTask(
      std::move(t), [&](Expected<HttpTaskCompletion, Status> resp) {
        EXPECT_FALSE(resp);
        EXPECT_FALSE(resp.error().ok());
        EXPECT_EQ(CURLE_OPERATION_TIMEDOUT, resp.error().code());
        EXPECT_EQ(curl_easy_strerror(CURLE_OPERATION_TIMEDOUT),
                  resp.error().message());
        l.count_down();
      });
  l.wait();
}

TEST_F(HttpEngineTest, Multi) {
  constexpr auto kCount = 1000;
  fiber::Latch l(kCount);
  for (int i = 0; i < kCount; ++i) {
    HttpTask t;
    t.SetMethod(HttpMethod::Get);
    t.SetUrl(site_url_ + "basic");
    t.SetTimeout(1s);
    HttpEngine::Instance()->StartTask(
        std::move(t), [&](Expected<HttpTaskCompletion, Status> resp) {
          EXPECT_TRUE(resp);
          EXPECT_EQ("hello world", FlattenSlow(*resp->body()));
          EXPECT_EQ("header_value",
                    GetHeader(*resp->headers(), "custom_header"));
          EXPECT_EQ(HttpStatus::OK, resp->status());
          EXPECT_EQ(HttpVersion::V_1_1, resp->version());
          l.count_down();
        });
  }
  l.wait();
}

}  // namespace flare::internal

FLARE_TEST_MAIN
