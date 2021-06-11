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

#include "flare/net/http/http_request.h"

#include <string_view>
#include <utility>

#include "googletest/gtest/gtest.h"

#include "flare/rpc/protocol/http/buffer_io.h"

namespace flare {

TEST(HttpRequest, Basic) {
  HttpRequest req;

  req.set_method(HttpMethod::Get);
  req.set_uri("/path/to/something");

  ASSERT_EQ(HttpMethod::Get, req.method());
  ASSERT_EQ("/path/to/something", req.uri());
}

TEST(HttpRequest, Swap) {
  HttpRequest req1, req2;

  req1.set_method(HttpMethod::Get);
  req1.set_uri("/path/to/something");
  req2.set_method(HttpMethod::Post);
  req2.set_uri("/empty");

  ASSERT_EQ(HttpMethod::Get, req1.method());
  ASSERT_EQ("/path/to/something", req1.uri());
  ASSERT_EQ(HttpMethod::Post, req2.method());
  ASSERT_EQ("/empty", req2.uri());

  std::swap(req1, req2);

  ASSERT_EQ(HttpMethod::Get, req2.method());
  ASSERT_EQ("/path/to/something", req2.uri());
  ASSERT_EQ(HttpMethod::Post, req1.method());
  ASSERT_EQ("/empty", req1.uri());
}

TEST(HttpRequest, Parsed) {
  static const auto kMsg =
      "GET / HTTP/1.1\r\n"
      "Accept-Language: zh-cn,zh-hk,zh-tw,en-us\r\n"
      "User-Agent: Sosospider+(+http://help.soso.com/webspider.htm)\r\n"
      "Accept-Encoding: gzip\r\n"
      "Connection: Keep-Alive\r\n"
      "Host: 192.0.2.1\r\n"
      "\r\n";
  auto buffer = CreateBufferSlow(kMsg);

  http::HeaderBlock block;
  ASSERT_EQ(http::ReadStatus::OK, http::ReadHeader(buffer, &block));

  std::string_view start_line;
  HttpVersion version;
  HttpMethod method;
  std::string_view uri;
  HttpRequest req;

  ASSERT_TRUE(
      http::ParseMessagePartial(std::move(block), &start_line, req.headers()));
  ASSERT_TRUE(http::ParseRequestStartLine(start_line, &version, &method, &uri));
  req.set_method(method);
  req.set_uri(std::string(uri));

  ASSERT_EQ("Sosospider+(+http://help.soso.com/webspider.htm)",
            *req.headers()->TryGet("User-Agent"));
  ASSERT_EQ(HttpMethod::Get, req.method());
}

TEST(HttpRequest, ParseHttp10) {
  static const auto kMsg =
      "GET / HTTP/1.0\r\n"
      "\r\n";
  auto buffer = CreateBufferSlow(kMsg);

  http::HeaderBlock block;
  ASSERT_EQ(http::ReadStatus::OK, http::ReadHeader(buffer, &block));

  std::string_view start_line;
  HttpVersion version;
  HttpMethod method;
  std::string_view uri;
  HttpRequest req;

  ASSERT_TRUE(
      http::ParseMessagePartial(std::move(block), &start_line, req.headers()));
  ASSERT_TRUE(http::ParseRequestStartLine(start_line, &version, &method, &uri));

  EXPECT_EQ(HttpVersion::V_1_0, version);
}

TEST(HttpRequest, ToString) {
  HttpRequest req;

  req.set_version(HttpVersion::V_1_1);
  req.set_uri("/path/to/home");
  req.set_method(HttpMethod::Put);
  req.headers()->Append("Hello", "World");
  req.set_body(CreateBufferSlow("something"));

  EXPECT_EQ(
      "PUT /path/to/home HTTP/1.1\r\n"
      "Hello: World\r\n"
      "\r\n"
      "something",
      req.ToString());
}

}  // namespace flare
