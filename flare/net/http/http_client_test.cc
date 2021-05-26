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

#include "flare/net/http/http_client.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <set>
#include <string>

#include "thirdparty/curl/curl.h"
#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/base/compression.h"
#include "flare/base/expected.h"
#include "flare/base/string.h"
#include "flare/base/thread/latch.h"
#include "flare/fiber/this_fiber.h"
#include "flare/init/override_flag.h"
#include "flare/rpc/http_handler.h"
#include "flare/rpc/protocol/http/message.h"  // For constants. FIXME.
#include "flare/rpc/server.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"

using namespace std::literals;

FLARE_OVERRIDE_FLAG(flare_http_client_default_timeout_ms, 10000);

namespace flare {

static Expected<HttpResponse, HttpClient::ErrorCode> Get(
    const std::string& url, std::chrono::nanoseconds timeout = 20s) {
  HttpClient client;
  HttpClient::RequestOptions opt{
      .timeout = timeout,
  };
  return client.Get(url, opt);
}

// TODO(luobogao): Use `base/internal/curl.cc` instead once available.
bool IsWanAccessible() {
  static const auto kResult = !!Get("https://baidu.com") &&
                              !!Get("https://qq.com") &&
                              !!Get("https://example.com");
  return kResult;
}

static Expected<HttpResponse, HttpClient::ErrorCode> Post(
    const std::string& url, std::string data,
    std::chrono::nanoseconds timeout = 100ms) {
  HttpClient client;
  HttpClient::RequestOptions opt{
      .timeout = timeout,
      .content_type = "application/octet-stream",
  };
  return client.Post(url, std::move(data), opt);
}

TEST(HttpClient, TestDomain) {
  if (!IsWanAccessible()) {
    FLARE_LOG_INFO("WAN is not accessible, skipping.");
    return;
  }
  auto resp = Get("https://example.com/");
  ASSERT_TRUE(resp);
  EXPECT_EQ(HttpStatus(200), resp->status());
}

TEST(HttpClient, TestNotFound) {
  if (!IsWanAccessible()) {
    FLARE_LOG_INFO("WAN is not accessible, skipping.");
    return;
  }
  auto resp = Get("https://example.com/404");
  ASSERT_TRUE(resp);
  EXPECT_EQ(HttpStatus(404), resp->status());
}

// Disabled: This UT won't fail as expected on CI due to HTTP proxy
// configured in that environment.
TEST(HttpClient, DISABLED_TestBadDomain) {
  auto resp = Get("http://non-exist.invalid-tld/");
  ASSERT_FALSE(resp);
  EXPECT_EQ(HttpClient::ERROR_FAIL_TO_RESOLVE_ADDRESS, resp.error());
}

TEST(HttpClient, DISABLED_TestTimeout) {
  auto resp = Get("http://127.0.0.1:1/");
  ASSERT_FALSE(resp);
  EXPECT_EQ(HttpClient::ERROR_CONNECTION, resp.error());
}

TEST(HttpClient, Https) {
  if (!IsWanAccessible()) {
    FLARE_LOG_INFO("WAN is not accessible, skipping.");
    return;
  }
  HttpClient client;
  auto res = client.Get("https://qq.com/");
  EXPECT_TRUE(res);
  EXPECT_EQ(HttpStatus(200), res->status());
}

TEST(HttpClient, HttpsWithBodySize) {
  if (!IsWanAccessible()) {
    FLARE_LOG_INFO("WAN is not accessible, skipping.");
    return;
  }
  HttpClient client;
  HttpClient::RequestOptions request_opts;
  std::string small_data(4 * 1024, 'A');
  auto&& res = client.Post("https://baidu.com/", small_data, request_opts);
  EXPECT_TRUE(res);
  EXPECT_EQ(HttpStatus(200), res->status());

  std::string big_data(4 * 1024 * 1024, 'A');
  res = client.Post("https://baidu.com/", big_data, request_opts);
  EXPECT_TRUE(res);
  EXPECT_EQ(HttpStatus(200), res->status());
}

TEST(HttpClient, DISABLED_Http2) {
  if (!IsWanAccessible()) {
    FLARE_LOG_INFO("WAN is not accessible, skipping.");
    return;
  }
  HttpClient client;
  HttpClient::ResponseInfo info;
  auto&& resp = client.Get("https://qq.com/", {}, &info);
  EXPECT_TRUE(resp);
  EXPECT_EQ(HttpStatus::OK, resp->status());
  EXPECT_EQ(HttpVersion::V_2, info.http_version);
}

TEST(HttpClient, OverrideHostType) {
  HttpClient client;
  HttpClient::RequestOptions opt{.override_host_nslb = "cl5"};
  auto&& resp = client.Get("https://123/", opt);
  EXPECT_FALSE(resp);
  EXPECT_EQ(HttpClient::ERROR_FAIL_TO_RESOLVE_ADDRESS, resp.error());
}

class EchoHandler : public HttpHandler {
 public:
  void HandleRequest(const HttpRequest& request, HttpResponse* response,
                     HttpServerContext* context) override {
    response->set_status(HttpStatus::OK);
    if (request.method() == HttpMethod::Get) {
      response->set_body("Get");
    } else {
      response->set_body(*request.body());
    }
  }
};

void AppendBodyChunked(HttpResponse* w, const std::string& chunk) {
  char size[9];
  snprintf(size, sizeof(size), "%X", static_cast<unsigned>(chunk.size()));
  w->body()->append(size);
  w->body()->append("\r\n");
  w->body()->append(chunk);
  w->body()->append("\r\n");
}

class HttpClientTest : public ::testing::Test {
 public:
  void SetUp() override {
    server_.AddProtocol("http");
    server_.AddHttpHandler("/", std::make_unique<EchoHandler>());
    server_.AddHttpHandler("/timeout",
                           NewHttpGetHandler([](auto&& r, auto&& w, auto&& c) {
                             this_fiber::SleepFor(200ms);
                             w->set_status(HttpStatus::OK);
                           }));
    server_.AddHttpHandler(
        "/short", NewHttpGetHandler([this](auto&& r, auto&& w, auto&& c) {
          w->headers()->Append("Connection", "close");
          w->set_body(std::to_string(count_++));
        }));
    server_.AddHttpHandler("/connection",
                           NewHttpGetHandler([](auto&& r, auto&& w, auto&& c) {
                             w->set_body(c->remote_peer.ToString());
                             this_fiber::SleepFor(2ms);
                             w->set_status(HttpStatus::OK);
                           }));
    server_.AddHttpHandler(
        "/redirect", NewHttpGetHandler([this](auto&& r, auto&& w, auto&& c) {
          w->set_status(HttpStatus::MovedPermanently);
          w->headers()->Append("Location", site_url_ + "redirect_again");
        }));
    server_.AddHttpHandler(
        "/redirect_again",
        NewHttpGetHandler([this](auto&& r, auto&& w, auto&& c) {
          w->set_status(HttpStatus::MovedPermanently);
          w->headers()->Append("Location", site_url_);
        }));
    server_.AddHttpHandler(
        "/redirect_self",
        NewHttpGetHandler([this](auto&& r, auto&& w, auto&& c) {
          w->set_status(HttpStatus::MovedPermanently);
          w->headers()->Append("Location", site_url_ + "redirect_self");
        }));
    server_.AddHttpHandler("/get_wanted_num_of_bytes",
                           NewHttpPostHandler([](auto&& r, auto&& w, auto&& c) {
                             w->set_status(HttpStatus::OK);
                             int wanted_size = *TryParse<int>(*r.body());
                             w->set_body(std::string(wanted_size, 'A'));
                           }));
    server_.AddHttpHandler("/no_content",
                           NewHttpGetHandler([](auto&& r, auto&& w, auto&& c) {
                             w->set_status(HttpStatus::NoContent);
                           }));
    server_.AddHttpHandler(
        "/chunked", NewHttpGetHandler([](auto&& r, auto&& w, auto&& c) {
          AppendBodyChunked(w, "1");
          AppendBodyChunked(w, "22");
          AppendBodyChunked(w, "333");
          AppendBodyChunked(w, "4444");
          AppendBodyChunked(w, "55555");
          AppendBodyChunked(w, "");
          w->headers()->Append("Transfer-Encoding", "chunked");
        }));
    server_.AddHttpHandler(
        "/chunked_timeout", NewHttpGetHandler([](auto&& r, auto&& w, auto&& c) {
          AppendBodyChunked(w, "1");
          w->headers()->Append("Transfer-Encoding", "chunked");
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
  std::atomic<int> count_ = 0;
};

TEST_F(HttpClientTest, Get) {
  auto resp = Get(site_url_);
  ASSERT_TRUE(resp);
  EXPECT_EQ(HttpVersion::V_1_1, resp->version());
  EXPECT_EQ(HttpStatus(200), resp->status());
  EXPECT_EQ("Get", *resp->body());
}

TEST_F(HttpClientTest, Post) {
  auto resp = Post(site_url_, "abc");
  ASSERT_TRUE(resp);
  EXPECT_EQ(HttpStatus(200), resp->status());
  EXPECT_EQ("abc", *resp->body());
}

TEST_F(HttpClientTest, ShortConnection) {
  for (int i = 0; i < 100; ++i) {
    auto resp = Get(site_url_ + "short");
    ASSERT_TRUE(resp);
    EXPECT_EQ(HttpStatus(200), resp->status());
    EXPECT_EQ(std::to_string(i), *resp->body());
  }
}

TEST_F(HttpClientTest, NoContent) {
  auto resp = Get(site_url_ + "no_content");
  ASSERT_TRUE(resp);
  EXPECT_EQ(HttpStatus(204), resp->status());
  EXPECT_TRUE(resp->body()->empty());
  EXPECT_FALSE(resp->headers()->TryGet(http::kContentLength));
}

TEST_F(HttpClientTest, AsyncGetShortConnection) {
  for (int j = 0; j != 10; ++j) {
    HttpClient client;
    Latch l(100);
    for (int i = 0; i < 100; ++i) {
      HttpClient::RequestOptions opt;
      opt.timeout = 10s;
      client.AsyncGet(site_url_ + "short", opt, nullptr)
          .Then([&](auto&& response) {
            EXPECT_TRUE(response);
            EXPECT_EQ(HttpStatus::OK, response->status());
            l.count_down();
          });
    }
    l.wait();
  }
}

TEST_F(HttpClientTest, AsyncRequest) {
  HttpClient client;
  HttpRequest request;
  request.set_method(HttpMethod::Get);
  request.headers()->Append("aaa", "aaaa");
  HttpResponse response;
  HttpClient::ResponseInfo response_info;
  // use 'localhost' instead of '127.0.0.1' will call domain resolve
  request.set_uri("/");
  HttpClient::RequestOptions opts;
  opts.headers = {"bbb:bbbb", "ccc:cccc"};
  auto resp = client.Request("http", "localhost:" + std::to_string(port_),
                             request, opts);
  ASSERT_TRUE(resp);
  ASSERT_EQ(HttpStatus(200), resp->status());
}

TEST_F(HttpClientTest, Timeout) {
  HttpClient::Options options;
  HttpClient client(options);
  HttpResponse response;
  ASSERT_EQ(HttpClient::ErrorCode::ERROR_TIMEOUT,
            client
                .Get(site_url_ + "timeout",
                     HttpClient::RequestOptions{.timeout = 100ms})
                .error());
}

TEST_F(HttpClientTest, NotTimeout) {
  HttpClient::Options options;
  HttpClient client(options);
  HttpResponse response;
  ASSERT_TRUE(client.Get(site_url_ + "timeout",
                         HttpClient::RequestOptions{.timeout = 500ms}));
}

constexpr int kRequestCount = 100;

TEST_F(HttpClientTest, MultiConnections) {
  HttpClient client;
  std::mutex mutex;
  Latch l(kRequestCount);
  std::set<std::string> addresses;
  for (int i = 0; i < kRequestCount; ++i) {
    HttpClient::RequestOptions opt;
    client.AsyncGet(site_url_ + "connection", opt, nullptr)
        .Then([&](auto&& response) {
          if (response) {
            std::unique_lock lk(mutex);
            addresses.insert(*response->body());
          }
          l.count_down();
        });
  }
  l.wait();
  EXPECT_GE(addresses.size(), 1U) << *addresses.begin();
  EXPECT_LE(addresses.size(), kRequestCount);
}

TEST_F(HttpClientTest, ErrorCodeToString) {
  for (int i = 0; i < 100; ++i) {
    HttpClient::ErrorCodeToString(i);
  }
}

TEST_F(HttpClientTest, Redirect) {
  // test too many redirect
  HttpClient::RequestOptions request_options;
  request_options.max_redirection_count = 1;
  HttpClient::ResponseInfo response_info;
  HttpClient client;

  {
    auto resp =
        client.Get(site_url_ + "redirect", request_options, &response_info);
    EXPECT_EQ(HttpClient::ERROR_TOO_MANY_REDIRECTS, resp.error());
  }

  // test redirect to self
  request_options.max_redirection_count = 2;
  HttpRequest request;
  request.set_method(HttpMethod::Get);

  {
    request.set_uri("/redirect_self");
    auto resp = client.Request("http", "localhost:" + std::to_string(port_),
                               request, request_options, &response_info);
    EXPECT_EQ(HttpClient::ERROR_TOO_MANY_REDIRECTS, resp.error());
  }

  // test normal situation
  {
    auto resp =
        client.Get(site_url_ + "redirect", request_options, &response_info);
    EXPECT_TRUE(resp);
    EXPECT_EQ(HttpStatus(200), resp->status());
    EXPECT_EQ("Get", *resp->body());
  }
  EXPECT_EQ(site_url_, response_info.effective_url);

  // Disable redirect
  HttpClient::Options client_opts;
  client_opts.follow_redirects = false;
  HttpClient client_disable_redirect(client_opts);
  {
    auto resp = client_disable_redirect.Get(site_url_ + "redirect",
                                            request_options, &response_info);
    ASSERT_TRUE(resp);
    EXPECT_EQ(HttpStatus::MovedPermanently, resp->status());
    EXPECT_EQ(site_url_ + "redirect_again",
              resp->headers()->TryGet("Location"));
  }
}

TEST_F(HttpClientTest, Compression) {
  HttpClient::Options opts;
  opts.use_builtin_compression = true;
  HttpClient client1(opts);
  opts.use_builtin_compression = false;
  HttpClient client2(opts);
  HttpClient::RequestOptions req_opts;
  auto&& resp =
      client1.Post(site_url_ + "get_wanted_num_of_bytes", "10", req_opts);
  EXPECT_TRUE(resp);
  EXPECT_EQ("gzip"sv, *resp->headers()->TryGet("Content-Encoding"));
  EXPECT_EQ(std::string(10, 'A'), *resp->body());
  resp = client2.Post(site_url_ + "get_wanted_num_of_bytes", "10", req_opts);
  EXPECT_TRUE(resp);
  EXPECT_FALSE(resp->headers()->TryGet("Content-Encoding"));
  EXPECT_EQ(std::string(10, 'A'), *resp->body());
  req_opts.headers = {"Accept-Encoding: snappy"};
  resp = client2.Post(site_url_ + "get_wanted_num_of_bytes", "10", req_opts);
  // Decompress ourselve
  EXPECT_TRUE(resp);
  EXPECT_EQ("snappy"sv, *resp->headers()->TryGet("Content-Encoding"));
  auto&& decompressed =
      Decompress(MakeDecompressor("snappy").get(), *resp->body());
  EXPECT_EQ(std::string(10, 'A'), FlattenSlow(*decompressed));
}

TEST_F(HttpClientTest, Chunked) {
  HttpClient client;
  auto&& resp = client.Get(site_url_ + "chunked");
  EXPECT_TRUE(resp);
  EXPECT_EQ("122333444455555", *resp->body());
  EXPECT_EQ("chunked", *resp->headers()->TryGet("Transfer-Encoding"));
}

TEST_F(HttpClientTest, ChunkedWithTimeout) {
  HttpClient client;
  HttpClient::RequestOptions opts;
  opts.timeout = 1s;
  auto&& resp = client.Get(site_url_ + "chunked_timeout", opts);
  EXPECT_FALSE(resp);
  EXPECT_EQ(HttpClient::ErrorCode::ERROR_TIMEOUT, resp.error());
}

}  // namespace flare

FLARE_TEST_MAIN
