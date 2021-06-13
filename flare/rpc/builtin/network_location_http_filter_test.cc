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

#include "flare/rpc/builtin/network_location_http_filter.h"

#include "gtest/gtest.h"

#include "flare/testing/main.h"

namespace flare {

TEST(NetworkLocationAllowOnHitHttpFilter, All) {
  NetworkLocationAllowOnHitHttpFilter filter({"192.0.2.1"});
  HttpRequest req;
  HttpResponse resp;
  HttpServerContext context;

  context.remote_peer = EndpointFromIpv4("192.0.2.1", 0);
  EXPECT_EQ(HttpFilter::Action::KeepProcessing,
            filter.OnFilter(&req, &resp, &context));
  context.remote_peer = EndpointFromIpv4("192.0.2.1", 1);
  EXPECT_EQ(HttpFilter::Action::KeepProcessing,
            filter.OnFilter(&req, &resp, &context));
  context.remote_peer = EndpointFromIpv4("192.0.2.2", 0);
  EXPECT_EQ(HttpFilter::Action::EarlyReturn,
            filter.OnFilter(&req, &resp, &context));
}

TEST(NetworkLocationBlockOnHitHttpFilter, All) {
  NetworkLocationBlockOnHitHttpFilter filter({"192.0.2.1"});
  HttpRequest req;
  HttpResponse resp;
  HttpServerContext context;

  context.remote_peer = EndpointFromIpv4("192.0.2.1", 0);
  EXPECT_EQ(HttpFilter::Action::EarlyReturn,
            filter.OnFilter(&req, &resp, &context));
  context.remote_peer = EndpointFromIpv4("192.0.2.1", 1);
  EXPECT_EQ(HttpFilter::Action::EarlyReturn,
            filter.OnFilter(&req, &resp, &context));
  context.remote_peer = EndpointFromIpv4("192.0.2.2", 0);
  EXPECT_EQ(HttpFilter::Action::KeepProcessing,
            filter.OnFilter(&req, &resp, &context));
}

}  // namespace flare

FLARE_TEST_MAIN
