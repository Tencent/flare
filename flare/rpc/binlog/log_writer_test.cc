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

#include "flare/rpc/binlog/log_writer.h"

#include <chrono>
#include <fstream>
#include <streambuf>
#include <thread>

#include "gflags/gflags.h"
#include "gtest/gtest.h"
#include "jsoncpp/json.h"

#include "flare/base/down_cast.h"
#include "flare/base/string.h"
#include "flare/init/override_flag.h"
#include "flare/rpc/binlog/testing.h"
#include "flare/rpc/binlog/text_only/dumper.h"
#include "flare/testing/main.h"

using namespace std::literals;

FLARE_OVERRIDE_FLAG(flare_binlog_dumper, "text_only");
FLARE_OVERRIDE_FLAG(flare_binlog_text_only_dumper_filename, "./dump.txt");

namespace flare::binlog {

TEST(LogWriter, Activated) {
  LogWriter writer(GetDumper());
  auto&& incoming = writer.GetIncomingCall();

  incoming->SetServiceName("fancy incoming service");
  incoming->SetOperationName("fancy incoming method");
  incoming->AddIncomingPacket(TestingPacketDesc("req1"));
  incoming->AddIncomingPacket(TestingPacketDesc("req2"));
  incoming->AddOutgoingPacket(TestingPacketDesc("resp1"));
  incoming->SetLocalPeer(EndpointFromIpv4("192.0.2.1", 1234));
  incoming->SetRemotePeer(EndpointFromIpv4("192.0.2.1", 1234));
  incoming->AddLog("my fancy log");

  for (int i = 0; i != 2; ++i) {
    auto&& outgoing = writer.StartOutgoingCall();
    outgoing->SetOperationName(Format("outgoing call #{}", i));
    outgoing->AddOutgoingPacket(TestingPacketDesc(Format("req1_{}", i)));
    outgoing->AddIncomingPacket(TestingPacketDesc(Format("req1_{}", i)));
    outgoing->SetUri(Format("http://my-fancy-uri-{}:5678", i));
    outgoing->AddLog(Format("my fancy log {}", i));
    outgoing->SetStartTimestamp({});
    outgoing->SetFinishTimestamp(std::chrono::steady_clock::time_point{1s});
  }
  incoming->SetStartTimestamp({});
  incoming->SetFinishTimestamp(std::chrono::steady_clock::time_point{1s});
  writer.Dump();

  std::this_thread::sleep_for(2s);  // Wait for DPC to run.

  Json::Value jsv;
  std::ifstream ifs("./dump.txt");
  ASSERT_TRUE(Json::Reader().parse(ifs, jsv));
  EXPECT_EQ("fancy incoming service",
            jsv["incomingCall"]["systemTags"]["service_name"].asString());
  EXPECT_EQ("fancy incoming method",
            jsv["incomingCall"]["systemTags"]["operation_name"].asString());
  EXPECT_EQ("outgoing call #0",
            jsv["outgoingCalls"][0]["systemTags"]["operation_name"].asString());
  EXPECT_EQ("http://my-fancy-uri-0:5678",
            jsv["outgoingCalls"][0]["systemTags"]["uri"].asString());
}

}  // namespace flare::binlog

FLARE_TEST_MAIN
