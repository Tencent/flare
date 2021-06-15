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

#include "flare/base/net/uri.h"

#include <string>

#include "gtest/gtest.h"

#include "flare/base/string.h"

// Adapted from `common/uri/uri_test.cc`

namespace flare {

TEST(Uri, Parse) {
  std::string uri_str =
      "http://www.baidu.com/"
      "s?tn=monline_dg&bs=DVLOG&f=8&wd=glog+DVLOG#fragment";
  auto parsed = TryParse<Uri>(uri_str);
  ASSERT_TRUE(parsed);

  EXPECT_EQ(uri_str, parsed->ToString());
  EXPECT_EQ("http", parsed->scheme());

  ASSERT_EQ("www.baidu.com", parsed->host());
  ASSERT_EQ(0, parsed->port());

  EXPECT_EQ("tn=monline_dg&bs=DVLOG&f=8&wd=glog+DVLOG", parsed->query());

  ASSERT_EQ("fragment", parsed->fragment());
  ASSERT_TRUE(TryParse<Uri>("http://l5(826753,65536)/monitro/es/dimeagg/"));
}

TEST(Uri, ParseAuthority) {
  std::string uristr =
      "http://username:password@127.0.0.1:8080/s?tn=monline_dg&bs=DVLOG";
  auto parsed = TryParse<Uri>(uristr);
  ASSERT_TRUE(parsed);
  EXPECT_EQ(uristr, parsed->ToString());
  EXPECT_EQ("http", parsed->scheme());

  ASSERT_EQ("/s", parsed->path());
  EXPECT_EQ("username:password", parsed->userinfo());
  EXPECT_EQ("127.0.0.1", parsed->host());
  EXPECT_EQ(8080, parsed->port());
}

TEST(Uri, ParseRelative) {
  const char* uristr = "/rpc?method=rpc_examples.EchoServer.Echo&format=json";
  auto parsed = TryParse<Uri>(uristr);
  ASSERT_TRUE(parsed);
  ASSERT_EQ("/rpc", parsed->path());
  ASSERT_EQ("method=rpc_examples.EchoServer.Echo&format=json", parsed->query());
}

TEST(Uri, BadUrl) {
  ASSERT_FALSE(TryParse<Uri>("http://^www.lianjiew.com/"));  // leading -
  ASSERT_FALSE(TryParse<Uri>("http://platform`info.py/"));  // domain contains _
  ASSERT_FALSE(TryParse<Uri>(" http://platform%info.py/"));  // leading space
}

}  // namespace flare
