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

#include "flare/net/http/query_string.h"

#include "googletest/gmock/gmock.h"
#include "googletest/gtest/gtest.h"

#include "flare/base/string.h"
#include "flare/net/http/http_request.h"

namespace flare {

TEST(QueryString, Parse) {
  auto parsed = TryParse<QueryString>("a=1&b=2&c=%FF%FE&d");

  ASSERT_EQ(4U, parsed->size());
  ASSERT_EQ("a", parsed->at(0).first);
  ASSERT_EQ("1", parsed->at(0).second);

  ASSERT_EQ("b", parsed->at(1).first);
  ASSERT_EQ("2", parsed->at(1).second);

  ASSERT_EQ("c", parsed->at(2).first);
  ASSERT_EQ("\xFF\xFE", parsed->at(2).second);

  ASSERT_EQ("d", parsed->at(3).first);
  ASSERT_EQ("", parsed->at(3).second);

  // Invalid pct-encoding.
  ASSERT_FALSE(TryParse<QueryString>("a=1&&b=%a"));
  // Invalid pct-encoding.
  ASSERT_FALSE(TryParse<QueryString>("a=1&&b=%km"));
}

TEST(QueryString, ParseUrl) {
  std::string url =
      "/json/task.json?task_type=kReduceTask&"
      "task_status=kRunningTask&offset=0&length=10";
  auto parsed = TryParseQueryStringFromUri(url);
  ASSERT_TRUE(parsed);
  auto task_type = parsed->TryGet("task_type");
  auto task_status = parsed->TryGet("task_status");
  EXPECT_EQ("kReduceTask", task_type);
  EXPECT_EQ("kRunningTask", task_status);

  EXPECT_EQ("0", parsed->TryGet("offset"));
  EXPECT_EQ("10", parsed->TryGet("length"));
  EXPECT_FALSE(parsed->TryGet("content"));
  EXPECT_EQ(10, parsed->TryGet<int>("length"));
}

TEST(QueryString, ParseHttpRequest) {
  HttpRequest request;
  request.set_method(HttpMethod::Post);
  request.set_uri("/?fromuri1=1&fromuri2=2");
  request.set_body("frombody1=1&frombody2=2");
  request.headers()->Append("Content-Type",
                            "application/x-www-form-urlencoded");
  auto parsed = TryParseQueryStringFromHttpRequest(request);
  EXPECT_EQ(1, parsed->TryGet<int>("fromuri1"));
  EXPECT_EQ(2, parsed->TryGet<int>("fromuri2"));
  EXPECT_EQ(1, parsed->TryGet<int>("frombody1"));
  EXPECT_EQ(2, parsed->TryGet<int>("frombody2"));
}

TEST(QueryString, ParseFromRequestIgnoreBody) {
  HttpRequest request;
  request.set_method(HttpMethod::Post);
  request.set_uri("/?fromuri1=1&fromuri2=2");
  request.set_body("frombody1=1&frombody2=2");
  auto parsed = TryParseQueryStringFromHttpRequest(request);
  EXPECT_EQ(1, parsed->TryGet<int>("fromuri1"));
  EXPECT_EQ(2, parsed->TryGet<int>("fromuri2"));
  EXPECT_FALSE(parsed->TryGet<int>("frombody1"));
  EXPECT_FALSE(parsed->TryGet<int>("frombody2"));
}

TEST(QueryString, ParseFromRequestIgnoreContentType) {
  HttpRequest request;
  request.set_method(HttpMethod::Post);
  request.set_uri("/?fromuri1=1&fromuri2=2");
  request.set_body("frombody1=1&frombody2=2");
  auto parsed = TryParseQueryStringFromHttpRequest(request, true);
  EXPECT_EQ(1, parsed->TryGet<int>("fromuri1"));
  EXPECT_EQ(2, parsed->TryGet<int>("fromuri2"));
  EXPECT_EQ(1, parsed->TryGet<int>("frombody1"));
  EXPECT_EQ(2, parsed->TryGet<int>("frombody2"));
}

TEST(QueryString, ToString) {
  const std::string kParamString = "a=1&b=2&c=%FF%FE&d=";
  auto parsed = TryParse<QueryString>(kParamString);
  ASSERT_TRUE(parsed);
  ASSERT_EQ(kParamString, parsed->ToString());
}

TEST(QueryString, BadlyEncoded) {
  auto parsed = TryParse<QueryString>("a=b=c");
  ASSERT_TRUE(parsed);
  ASSERT_EQ("b=c", parsed->TryGet("a"));
}

}  // namespace flare
