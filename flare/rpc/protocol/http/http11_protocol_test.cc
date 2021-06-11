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

#include "flare/rpc/protocol/http/http11_protocol.h"

#include "googletest/gtest/gtest.h"

#include "flare/rpc/protocol/http/message.h"
#include "flare/testing/main.h"

using namespace std::literals;

namespace flare::http {

TEST(Http11ServerSideProtocolTest, TestCutMesssage) {
  Http11Protocol server_http_prot(true);

  {
    // Request without content
    auto legal_req_content =
        "GET / HTTP/1.1\r\n"
        "Host: 192.0.2.1\r\n"
        "\r\n"sv;
    auto buffer = CreateBufferSlow(legal_req_content);
    std::unique_ptr<Message> cut_msg;
    ASSERT_EQ(StreamProtocol::MessageCutStatus::Cut,
              server_http_prot.TryCutMessage(&buffer, &cut_msg));
    EXPECT_TRUE(!!cut_msg);
    ASSERT_TRUE(server_http_prot.TryParse(
        &cut_msg,
        server_http_prot.GetControllerFactory()->Create(false).get()));
    auto request_msg = dyn_cast<HttpRequestMessage>(cut_msg.get());
    ASSERT_TRUE(!!request_msg);
    EXPECT_EQ(HttpMethod::Get, request_msg->request()->method());
  }
  {
    // request with content
    auto legal_req_content =
        "POST / HTTP/1.1\r\n"
        "Content-Length: 8\r\n"
        "\r\n"sv;
    NoncontiguousBuffer buffer;
    buffer.Append(CreateBufferSlow(legal_req_content));
    std::unique_ptr<Message> cut_msg;
    EXPECT_EQ(StreamProtocol::MessageCutStatus::NeedMore,
              server_http_prot.TryCutMessage(&buffer, &cut_msg));
    EXPECT_FALSE(!!cut_msg);
    // partial body arrived
    buffer.Append(CreateBufferSlow("dead"sv));
    EXPECT_EQ(StreamProtocol::MessageCutStatus::NeedMore,
              server_http_prot.TryCutMessage(&buffer, &cut_msg));
    EXPECT_FALSE(!!cut_msg);
    // body complete
    buffer.Append(CreateBufferSlow("beef"sv));
    EXPECT_EQ(StreamProtocol::MessageCutStatus::Cut,
              server_http_prot.TryCutMessage(&buffer, &cut_msg));
    EXPECT_TRUE(buffer.Empty());
    EXPECT_TRUE(!!cut_msg);
    ASSERT_TRUE(server_http_prot.TryParse(
        &cut_msg,
        server_http_prot.GetControllerFactory()->Create(false).get()));
    auto request_msg = dyn_cast<HttpRequestMessage>(cut_msg.get());
    EXPECT_EQ("8", *request_msg->request()->headers()->TryGet(kContentLength));
    EXPECT_EQ("deadbeef", *request_msg->request()->body());
  }
  {
    // One byte by one byte.
    const auto PostRequest =
        "POST /path/to/echo.svc HTTP/1.1\r\n"
        "User-Agent: curl/7.29.0\r\n"
        "Host: localhost:8888\r\n"
        "Accept: */*\r\n"
        "Content-Length: 6\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "\r\n"
        "123456"sv;
    NoncontiguousBuffer buffer;
    std::unique_ptr<Message> cut_msg;
    for (int i = 0; i != PostRequest.size() - 7; ++i) {
      buffer.Append(CreateBufferSlow(PostRequest.substr(i, 1)));
      EXPECT_EQ(StreamProtocol::MessageCutStatus::NotIdentified,
                server_http_prot.TryCutMessage(&buffer, &cut_msg));
      EXPECT_FALSE(!!cut_msg);
    }
    buffer.Append(
        CreateBufferSlow(PostRequest.substr(PostRequest.size() - 7, 1)));
    EXPECT_EQ(StreamProtocol::MessageCutStatus::NeedMore,
              server_http_prot.TryCutMessage(&buffer, &cut_msg));
    EXPECT_FALSE(!!cut_msg);
    for (int i = PostRequest.size() - 6; i != PostRequest.size() - 1; ++i) {
      buffer.Append(CreateBufferSlow(PostRequest.substr(i, 1)));
      EXPECT_EQ(StreamProtocol::MessageCutStatus::NeedMore,
                server_http_prot.TryCutMessage(&buffer, &cut_msg));
      EXPECT_FALSE(!!cut_msg);
    }
    buffer.Append(CreateBufferSlow(PostRequest.substr(PostRequest.size() - 1)));
    EXPECT_EQ(StreamProtocol::MessageCutStatus::Cut,
              server_http_prot.TryCutMessage(&buffer, &cut_msg));
    EXPECT_TRUE(buffer.Empty());
    EXPECT_TRUE(!!cut_msg);
    ASSERT_TRUE(server_http_prot.TryParse(
        &cut_msg,
        server_http_prot.GetControllerFactory()->Create(false).get()));
    auto request_msg = dyn_cast<HttpRequestMessage>(cut_msg.get());
    EXPECT_EQ("6", *request_msg->request()->headers()->TryGet(kContentLength));
    EXPECT_EQ("123456", *request_msg->request()->body());
  }
  {
    // No method found in request.
    auto illegal_req_content =
        "/ HTTP/1.1\r\n"
        "Host: 192.0.2.1\r\n"
        "\r\n"sv;
    auto buffer = CreateBufferSlow(illegal_req_content);
    std::unique_ptr<Message> cut_msg;
    EXPECT_EQ(StreamProtocol::MessageCutStatus::ProtocolMismatch,
              server_http_prot.TryCutMessage(&buffer, &cut_msg));
    EXPECT_FALSE(!!cut_msg);
  }
}

}  // namespace flare::http

FLARE_TEST_MAIN
