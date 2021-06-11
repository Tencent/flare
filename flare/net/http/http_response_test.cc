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

#include "flare/net/http/http_response.h"

#include <string>
#include <string_view>
#include <utility>

#include "googletest/gtest/gtest.h"

#include "flare/rpc/protocol/http/buffer_io.h"

using namespace std::literals;

namespace flare {

TEST(HttpResponse, Basic) {
  HttpResponse resp;

  resp.set_status(HttpStatus::OK);
  ASSERT_EQ(HttpStatus::OK, resp.status());
}

TEST(HttpResponse, Swap) {
  HttpResponse resp1, resp2;

  resp1.set_status(HttpStatus::OK);
  resp2.set_status(HttpStatus::NotFound);
  ASSERT_EQ(HttpStatus::OK, resp1.status());
  ASSERT_EQ(HttpStatus::NotFound, resp2.status());

  std::swap(resp1, resp2);
  ASSERT_EQ(HttpStatus::OK, resp2.status());
  ASSERT_EQ(HttpStatus::NotFound, resp1.status());
}

TEST(HttpResponse, Parsed) {
  static const auto kHeader =
      "HTTP/1.1 200 OK\r\n"
      "Date: Mon, 16 Jan 2012 06:41:37 GMT\r\n"
      "Server: Apache/2.2.21 (Unix)\r\n"
      "Last-Modified: Wed, 11 Jan 2012 09:15:13 GMT\r\n"
      "Accept-Ranges: bytes\r\n"
      "Content-Length: 45\r\n"
      "Keep-Alive: timeout=5, max=100\r\n"
      "Connection: Keep-Alive\r\n"
      "Content-Type: text/html\r\n"
      "\r\n";
  static const auto kMsg = kHeader + std::string(45, 'a');
  auto buffer = CreateBufferSlow(kMsg);

  http::HeaderBlock block;
  ASSERT_EQ(http::ReadStatus::OK, http::ReadHeader(buffer, &block));
  buffer.Skip(block.second);

  std::string_view start_line;
  HttpStatus status;
  HttpResponse resp;

  ASSERT_TRUE(
      http::ParseMessagePartial(std::move(block), &start_line, resp.headers()));
  ASSERT_TRUE(http::ParseResponseStartLine(start_line, &status));
  resp.set_status(status);
  resp.set_body(std::move(buffer));

  ASSERT_EQ(HttpStatus::OK, resp.status());
  ASSERT_EQ("Mon, 16 Jan 2012 06:41:37 GMT", *resp.headers()->TryGet("Date"));
  ASSERT_EQ(45, *resp.headers()->TryGet<int>("Content-Length"));
  ASSERT_EQ(std::string(45, 'a'), *resp.body());
}

TEST(HttpResponse, ToString) {
  HttpResponse resp;

  resp.set_version(HttpVersion::V_1_1);
  resp.set_status(HttpStatus::ImATeapot);
  resp.headers()->Append("Hello", "World");
  resp.set_body(CreateBufferSlow("something"));

  EXPECT_EQ(
      "HTTP/1.1 418 I'm a teapot\r\n"
      "Hello: World\r\n"
      "\r\n"
      "something",
      resp.ToString());
}

}  // namespace flare
