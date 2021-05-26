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

#include "flare/testing/http_mock.h"

#include "flare/fiber/future.h"
#include "flare/net/http/http_client.h"
#include "flare/testing/main.h"

using namespace std::literals;

using testing::_;
using testing::HasSubstr;
using testing::Ne;
using testing::Not;
using testing::StrEq;
using testing::StrNe;

namespace flare::testing::detail {

TEST(HttpMockTest, HttpSucc) {
  HttpClient c;

  HttpResponse resp;
  resp.set_body("123");
  FLARE_EXPECT_HTTP(_, _, _, _).WillRepeatedly(Return(resp));
  std::string url = "mock://asdasd";

  auto&& e = c.Get(url);
  EXPECT_TRUE(e);
  EXPECT_EQ("123", *e->body());

  e = fiber::BlockingGet(c.AsyncGet(url));
  EXPECT_TRUE(e);
  EXPECT_EQ("123", *e->body());

  e = c.Post(url, "", {});
  EXPECT_TRUE(e);
  EXPECT_EQ("123", *e->body());

  e = fiber::BlockingGet(c.AsyncPost(url, "", {}));
  EXPECT_TRUE(e);
  EXPECT_EQ("123", *e->body());

  HttpRequest req;
  e = c.Request("mock", "", req, {});
  EXPECT_TRUE(e);
  EXPECT_EQ("123", *e->body());

  e = fiber::BlockingGet(c.AsyncRequest("mock", "", req, {}));
  EXPECT_TRUE(e);
  EXPECT_EQ("123", *e->body());
}

TEST(HttpMockTest, HttpFail) {
  HttpClient c;

  HttpClient::ErrorCode err = HttpClient::ERROR_CONNECTION;
  FLARE_EXPECT_HTTP(_, _, _, _).WillRepeatedly(Return(err));
  std::string url = "mock://asdasd";

  auto&& e = c.Get(url);
  EXPECT_FALSE(e);
  EXPECT_EQ(err, e.error());

  e = fiber::BlockingGet(c.AsyncGet(url));
  EXPECT_FALSE(e);
  EXPECT_EQ(err, e.error());

  e = c.Post(url, "", {});
  EXPECT_FALSE(e);
  EXPECT_EQ(err, e.error());

  e = fiber::BlockingGet(c.AsyncPost(url, "", {}));
  EXPECT_FALSE(e);
  EXPECT_EQ(err, e.error());

  HttpRequest req;
  e = c.Request("mock", "", {});
  EXPECT_FALSE(e);
  EXPECT_EQ(err, e.error());

  e = fiber::BlockingGet(c.AsyncRequest("mock", "", {}));
  EXPECT_FALSE(e);
  EXPECT_EQ(err, e.error());
}

TEST(HttpMockTest, HttpMatchUrl) {
  HttpClient c;

  HttpResponse resp;
  std::string url = "mock://asdasd";
  HttpClient::ErrorCode err = HttpClient::ERROR_CONNECTION;
  FLARE_EXPECT_HTTP(url, _, _, _).WillRepeatedly(Return(resp));
  FLARE_EXPECT_HTTP(StrNe(url), _, _, _).WillRepeatedly(Return(err));
  auto&& e = c.Get(url);
  EXPECT_TRUE(e);
  e = c.Get(url + "blabla");
  EXPECT_FALSE(e);
}

TEST(HttpMockTest, HttpMatchMethod) {
  HttpClient c;

  HttpResponse resp;
  std::string url = "mock://asdasd";
  HttpClient::ErrorCode err = HttpClient::ERROR_CONNECTION;
  FLARE_EXPECT_HTTP(_, HttpMethod::Get, _, _).WillRepeatedly(Return(resp));
  FLARE_EXPECT_HTTP(_, Ne(HttpMethod::Get), _, _).WillRepeatedly(Return(err));
  auto&& e = c.Get(url);
  EXPECT_TRUE(e);
  e = c.Post(url, "", {});
  EXPECT_FALSE(e);
}

TEST(HttpMockTest, HttpMatchBody) {
  HttpClient c;

  HttpResponse resp;
  std::string url = "mock://asdasd";
  HttpClient::ErrorCode err = HttpClient::ERROR_CONNECTION;
  FLARE_EXPECT_HTTP(_, _, _, HasSubstr("123")).WillRepeatedly(Return(resp));
  FLARE_EXPECT_HTTP(_, _, _, StrNe("123")).WillRepeatedly(Return(err));
  auto&& e = c.Post(url, "123", {});
  EXPECT_TRUE(e);
  e = c.Post(url, "456", {});
  EXPECT_FALSE(e);
}

TEST(HttpMockTest, HttpMatchHeaderContain) {
  HttpClient c;

  HttpResponse resp;
  std::string url = "mock://asdasd";
  HttpClient::ErrorCode err = HttpClient::ERROR_CONNECTION;
  FLARE_EXPECT_HTTP(_, _, HttpHeaderContains("aaa"), _)
      .WillRepeatedly(Return(resp));
  FLARE_EXPECT_HTTP(_, _, Not(HttpHeaderContains("aaa")), _)
      .WillRepeatedly(Return(err));
  HttpRequest req;
  req.headers()->Append("aaa", "val");
  auto&& e = c.Request("mock", "", req);
  EXPECT_TRUE(e);
  req.headers()->Remove("aaa");
  e = c.Request("mock", "", req);
  EXPECT_FALSE(e);
}

TEST(HttpMockTest, HttpMatchHeaderEq) {
  HttpClient c;

  HttpResponse resp;
  std::string url = "mock://asdasd";
  HttpClient::ErrorCode err = HttpClient::ERROR_CONNECTION;
  FLARE_EXPECT_HTTP(_, _, HttpHeaderEq("aaa", "val"), _)
      .WillRepeatedly(Return(resp));
  FLARE_EXPECT_HTTP(_, _, Not(HttpHeaderEq("aaa", "val")), _)
      .WillRepeatedly(Return(err));
  HttpRequest req;
  req.headers()->Append("aaa", "val");
  auto&& e = c.Request("mock", "", req);
  EXPECT_TRUE(e);
  req.headers()->Set("aaa", "lalala");
  e = c.Request("mock", "", req);
  EXPECT_FALSE(e);
}

TEST(HttpMockTest, HttpFillResponseInfo) {
  HttpClient c;

  HttpResponse resp;
  std::string url = "mock://asdasd";
  HttpClient::ResponseInfo info;
  info.effective_url = "blabla";
  FLARE_EXPECT_HTTP(_, _, _, _).WillRepeatedly(Return(resp, info));
  HttpClient::ResponseInfo result_info;
  auto&& e = c.Get(url, {}, &result_info);
  EXPECT_TRUE(e);
  EXPECT_EQ(info.effective_url, result_info.effective_url);
}

}  // namespace flare::testing::detail

FLARE_TEST_MAIN
