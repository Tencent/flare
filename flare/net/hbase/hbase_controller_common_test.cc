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

#include "flare/net/hbase/hbase_controller_common.h"

#include <chrono>
#include <utility>

#include "googletest/gtest/gtest.h"

#include "flare/base/chrono.h"

using namespace std::literals;

namespace flare::hbase {

class MyController : public HbaseControllerCommon {
  // `HbaseControllerCommon` is intended to be used as a base class, so we
  // implements all its virtual methods here.
};

TEST(HbaseControllerCommon, Exception) {
  HbaseException xcpt;
  xcpt.set_exception_class_name("my xcpt");

  MyController ctlr;
  ctlr.SetException(std::move(xcpt));
  ASSERT_EQ("my xcpt", ctlr.GetException().exception_class_name());
  ASSERT_TRUE(ctlr.Failed());
  ASSERT_EQ("my xcpt", ctlr.ErrorText());
  // Not moved away.
  ASSERT_EQ("my xcpt", ctlr.GetException().exception_class_name());

  ctlr.Reset();
  ASSERT_FALSE(ctlr.Failed());
  ASSERT_EQ("", ctlr.GetException().exception_class_name());
  ASSERT_EQ("", ctlr.ErrorText());
}

TEST(HbaseControllerCommon, RequestCellBlock) {
  MyController ctlr;

  ctlr.SetRequestCellBlock(
      CreateBufferSlow(std::string(131072, 'a') + "bcdef"));
  ASSERT_EQ(std::string(131072, 'a') + "bcdef",
            FlattenSlow(ctlr.GetRequestCellBlock()));

  ctlr.Reset();
  ASSERT_TRUE(ctlr.GetRequestCellBlock().Empty());
}

TEST(HbaseControllerCommon, ResponseCellBlock) {
  MyController ctlr;

  ctlr.SetResponseCellBlock(
      CreateBufferSlow(std::string(131072, 'a') + "bcdef"));
  ASSERT_EQ(std::string(131072, 'a') + "bcdef",
            FlattenSlow(ctlr.GetResponseCellBlock()));

  ctlr.Reset();
  ASSERT_TRUE(ctlr.GetResponseCellBlock().Empty());
}

TEST(HbaseControllerCommon, Timeout) {
  MyController ctlr;

  ctlr.SetTimeout(2s);
  ASSERT_NEAR(2s / 1ms, (ctlr.GetTimeout() - ReadSteadyClock()) / 1ms, 10);
  ctlr.SetTimeout(ReadSteadyClock() + 1s);
  ASSERT_NEAR(1s / 1ms, (ctlr.GetTimeout() - ReadSteadyClock()) / 1ms, 10);
  ctlr.SetTimeout(ReadSystemClock() + 3s);
  ASSERT_NEAR(3s / 1ms, (ctlr.GetTimeout() - ReadSteadyClock()) / 1ms, 10);
  ctlr.SetTimeout(std::chrono::high_resolution_clock::now() + 5s);
  ASSERT_NEAR(5s / 1ms, (ctlr.GetTimeout() - ReadSteadyClock()) / 1ms, 10);

  ctlr.Reset();
  ASSERT_NEAR(2s / 1ms, (ctlr.GetTimeout() - ReadSteadyClock()) / 1ms, 10);
}

TEST(HbaseControllerCommon, RemotePeer) {
  MyController ctlr;

  ctlr.SetRemotePeer(EndpointFromIpv4("192.0.2.1", 1234));
  ASSERT_EQ(EndpointFromIpv4("192.0.2.1", 1234), ctlr.GetRemotePeer());
  ctlr.SetRemotePeer(EndpointFromIpv6("2001:db8::1", 56789));
  ASSERT_EQ(EndpointFromIpv6("2001:db8::1", 56789), ctlr.GetRemotePeer());
}

}  // namespace flare::hbase
