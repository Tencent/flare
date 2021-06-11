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

#include "flare/net/http/utility.h"

#include "googletest/gtest/gtest.h"

#include "flare/base/string.h"
#include "flare/net/http/http_request.h"

namespace flare {

TEST(TryGetOriginatingIp, All) {
  HttpRequest req;
  req.headers()->Append("x-forwarded-for", "2001:db8::1");
  auto ip = TryGetOriginatingIp(req);
  ASSERT_TRUE(ip);
  EXPECT_EQ("2001:db8::1", *ip);
}

TEST(TryGetOriginatingEndpoint, All) {
  HttpRequest req;
  req.headers()->Append("X-Real-IP", "2001:db8::1");
  auto ep = TryGetOriginatingEndpoint(req);
  ASSERT_TRUE(ep);
  EXPECT_EQ("2001:db8::1", EndpointGetIp(*ep));
}

}  // namespace flare
