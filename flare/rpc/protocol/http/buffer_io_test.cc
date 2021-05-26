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

#include "flare/rpc/protocol/http/buffer_io.h"

#include <string>

#include "thirdparty/googletest/gmock/gmock.h"
#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/base/buffer.h"
#include "flare/net/http/http_headers.h"
#include "flare/net/http/http_request.h"
#include "flare/net/http/http_response.h"

using namespace std::literals;

namespace flare::http {

TEST(ReadHeaderAndParse, Easy) {
  static const auto header =
      "GET / HTTP/1.1\r\n"
      "Content-Type:    application/json\r\n"
      "Content-Length: 10   \r\n"
      "X-Empty-Header:    \r\n"
      "X-Duplicate-Header:   1\r\n"
      "X-Duplicate-Header: 2\r\n"
      "\r\n"s;
  NoncontiguousBuffer buffer;

  buffer.Append(CreateBufferSlow(header));
  HeaderBlock header_block;
  ASSERT_EQ(ReadStatus::OK, ReadHeader(buffer, &header_block));

  HttpHeaders headers;
  std::string_view start_line;
  ASSERT_TRUE(
      ParseMessagePartial(std::move(header_block), &start_line, &headers));
  ASSERT_EQ("10", *headers.TryGet("Content-Length"));
  ASSERT_EQ("application/json", *headers.TryGet("Content-Type"));
  ASSERT_EQ("", *headers.TryGet("X-Empty-Header"));
  ASSERT_THAT(headers.TryGetMultiple("X-Duplicate-Header"),
              ::testing::ElementsAre("1", "2"));
  ASSERT_EQ(
      "Content-Type: application/json\r\n"
      "Content-Length: 10\r\n"
      "X-Empty-Header: \r\n"
      "X-Duplicate-Header: 1\r\n"
      "X-Duplicate-Header: 2\r\n",
      headers.ToString());
  ASSERT_EQ("GET / HTTP/1.1", start_line);
}

TEST(ReadHeaderAndParse, Normal) {
  NoncontiguousBuffer buffer;
  HeaderBlock header_block;

  buffer.Append(CreateBufferSlow("HTTP/1.1 "));
  buffer.Append(CreateBufferSlow("B"));
  buffer.Append(CreateBufferSlow("o"));
  buffer.Append(CreateBufferSlow("r"));
  buffer.Append(CreateBufferSlow("i"));
  buffer.Append(CreateBufferSlow("n"));
  buffer.Append(CreateBufferSlow("g"));
  buffer.Append(CreateBufferSlow(" "));
  buffer.Append(CreateBufferSlow("l"));
  buffer.Append(CreateBufferSlow("i"));
  buffer.Append(CreateBufferSlow("n"));
  buffer.Append(CreateBufferSlow("e\r"));
  buffer.Append(CreateBufferSlow("\nContent-Type"));
  ASSERT_EQ(ReadStatus::NoEnoughData, ReadHeader(buffer, &header_block));
  buffer.Append(CreateBufferSlow(":"));
  ASSERT_EQ(ReadStatus::NoEnoughData, ReadHeader(buffer, &header_block));
  buffer.Append(CreateBufferSlow("application/json"));
  ASSERT_EQ(ReadStatus::NoEnoughData, ReadHeader(buffer, &header_block));
  buffer.Append(CreateBufferSlow("\r"));
  ASSERT_EQ(ReadStatus::NoEnoughData, ReadHeader(buffer, &header_block));
  buffer.Append(CreateBufferSlow("\n"));
  ASSERT_EQ(ReadStatus::NoEnoughData, ReadHeader(buffer, &header_block));
  buffer.Append(CreateBufferSlow("Content-Length:"));
  ASSERT_EQ(ReadStatus::NoEnoughData, ReadHeader(buffer, &header_block));
  buffer.Append(CreateBufferSlow(" 10     \r"));
  ASSERT_EQ(ReadStatus::NoEnoughData, ReadHeader(buffer, &header_block));
  buffer.Append(CreateBufferSlow("\n"));
  buffer.Append(CreateBufferSlow("X-Empty-"));
  buffer.Append(CreateBufferSlow("Heade"));
  buffer.Append(CreateBufferSlow("r:"));
  ASSERT_EQ(ReadStatus::NoEnoughData, ReadHeader(buffer, &header_block));
  buffer.Append(CreateBufferSlow("\r"));
  ASSERT_EQ(ReadStatus::NoEnoughData, ReadHeader(buffer, &header_block));
  buffer.Append(CreateBufferSlow("\n"));
  buffer.Append(CreateBufferSlow("X-Duplicate-Header:   1\r\n"));
  buffer.Append(CreateBufferSlow("X-Duplicate-Header: 2  \r\n"));
  ASSERT_EQ(ReadStatus::NoEnoughData, ReadHeader(buffer, &header_block));
  buffer.Append(CreateBufferSlow("\r"));
  ASSERT_EQ(ReadStatus::NoEnoughData, ReadHeader(buffer, &header_block));
  buffer.Append(CreateBufferSlow("\n"));
  ASSERT_EQ(ReadStatus::OK, ReadHeader(buffer, &header_block));

  HttpHeaders headers;
  std::string_view start_line;
  ASSERT_TRUE(
      ParseMessagePartial(std::move(header_block), &start_line, &headers));
  ASSERT_EQ("10", *headers.TryGet("Content-Length"));
  ASSERT_EQ("application/json", *headers.TryGet("Content-Type"));
  ASSERT_EQ("", *headers.TryGet("X-Empty-Header"));
  ASSERT_THAT(headers.TryGetMultiple("X-Duplicate-Header"),
              ::testing::ElementsAre("1", "2"));
  ASSERT_EQ(
      "Content-Type: application/json\r\n"
      "Content-Length: 10\r\n"
      "X-Empty-Header: \r\n"
      "X-Duplicate-Header: 1\r\n"
      "X-Duplicate-Header: 2\r\n",
      headers.ToString());
  ASSERT_EQ("HTTP/1.1 Boring line", start_line);
}

TEST(ReadHeader, NoEnoughData) {
  NoncontiguousBuffer buffer;
  HeaderBlock header_block;

  buffer.Append(CreateBufferSlow("GET / HTTP/1.1\r\nContent-Type"));
  ASSERT_EQ(ReadStatus::NoEnoughData, ReadHeader(buffer, &header_block));
}

TEST(ReadHeader, NoEnoughData2) {
  NoncontiguousBuffer buffer;
  HeaderBlock header_block;

  buffer.Append(CreateBufferSlow("GET / HTTP/1.1\r\nContent-Type:aaa\r\n:"));
  ASSERT_EQ(ReadStatus::NoEnoughData, ReadHeader(buffer, &header_block));
}

TEST(ParseMessagePartial, Malformed0) {
  NoncontiguousBuffer buffer;
  HeaderBlock header_block;

  buffer.Append(CreateBufferSlow("GET / HTTP/1.1\r\nContent-Type"));
  buffer.Append(CreateBufferSlow("\r\n\r\n"));
  ASSERT_EQ(ReadStatus::OK, ReadHeader(buffer, &header_block));

  std::string_view start_line;
  HttpHeaders headers;
  ASSERT_FALSE(
      ParseMessagePartial(std::move(header_block), &start_line, &headers));
}

TEST(ParseMessagePartial, Malformed1) {
  NoncontiguousBuffer buffer;
  HeaderBlock header_block;

  buffer.Append(
      CreateBufferSlow("GET / HTTP/1.1\r\n"
                       "Content-Type\r\n\r\n"
                       "Content-Type:aaa\r\n"
                       "Content-Length:123\r\n\r\n"));
  ASSERT_EQ(ReadStatus::OK, ReadHeader(buffer, &header_block));

  std::string_view start_line;
  HttpHeaders headers;
  ASSERT_FALSE(
      ParseMessagePartial(std::move(header_block), &start_line, &headers));
}

TEST(ParseMessagePartial, Malformed2) {
  NoncontiguousBuffer buffer;
  HeaderBlock header_block;

  buffer.Append(CreateBufferSlow("GET / HTTP/1.1\r\n:\r\n\r\n"));
  ASSERT_EQ(ReadStatus::OK, ReadHeader(buffer, &header_block));

  std::string_view start_line;
  HttpHeaders headers;
  ASSERT_FALSE(
      ParseMessagePartial(std::move(header_block), &start_line, &headers));
}

TEST(ParseMessagePartial, Malformed3) {
  NoncontiguousBuffer buffer;
  HeaderBlock header_block;

  buffer.Append(
      CreateBufferSlow("GET / HTTP/1.1\r\nContent-Type:aaa\r\n:\r\n\r\n"));
  ASSERT_EQ(ReadStatus::OK, ReadHeader(buffer, &header_block));

  std::string_view start_line;
  HttpHeaders headers;
  ASSERT_FALSE(
      ParseMessagePartial(std::move(header_block), &start_line, &headers));
}

TEST(ParseMessagePartial, NoHttpHeaders) {
  NoncontiguousBuffer buffer;
  HeaderBlock header_block;

  buffer.Append(CreateBufferSlow("GET / HTTP/1.1\r\n\r\n"));
  ASSERT_EQ(ReadStatus::OK, ReadHeader(buffer, &header_block));
  HttpHeaders headers;
  std::string_view start_line;
  ASSERT_TRUE(
      ParseMessagePartial(std::move(header_block), &start_line, &headers));
  ASSERT_EQ("", headers.ToString());
  ASSERT_EQ("GET / HTTP/1.1", start_line);
}

TEST(ReadHeader, EntityTooLarge) {
  NoncontiguousBuffer buffer;
  HeaderBlock header_block;

  buffer.Append(CreateBufferSlow("GET / HTTP/1.1" + std::string(1000000, 'c')));
  ASSERT_EQ(ReadStatus::Error, ReadHeader(buffer, &header_block));
}

TEST(WriteMessage, Request) {
  HttpRequest request;
  request.set_version(HttpVersion::V_1_0);
  request.set_method(HttpMethod::Post);
  request.set_uri("/path/to/something");
  request.set_body("body123456");
  request.headers()->Append("X-Duplicate-Header", "1");
  request.headers()->Append("X-Duplicate-Header", "2");
  request.headers()->Append("Content-Length", "10");
  NoncontiguousBufferBuilder builder;
  WriteMessage(request, &builder);
  ASSERT_EQ(
      "POST /path/to/something HTTP/1.0\r\n"
      "X-Duplicate-Header: 1\r\n"
      "X-Duplicate-Header: 2\r\n"
      "Content-Length: 10\r\n"
      "\r\n"
      "body123456",
      FlattenSlow(builder.DestructiveGet()));
}

TEST(WriteMessage, Response) {
  HttpResponse response;
  response.set_version(HttpVersion::V_1_1);
  response.set_status(HttpStatus::OK);
  response.set_body("123");
  response.headers()->Append("X-Response-Header", "1");
  response.headers()->Append("X-Response-Header", "2");
  response.headers()->Append("Content-Length", "3");
  NoncontiguousBufferBuilder builder;
  WriteMessage(response, &builder);
  ASSERT_EQ(
      "HTTP/1.1 200 OK\r\n"
      "X-Response-Header: 1\r\n"
      "X-Response-Header: 2\r\n"
      "Content-Length: 3\r\n"
      "\r\n"
      "123",
      FlattenSlow(builder.DestructiveGet()));
}

}  // namespace flare::http
