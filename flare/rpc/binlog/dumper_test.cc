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

#include "flare/rpc/binlog/dumper.h"

#include <chrono>
#include <fstream>
#include <thread>

#include "gflags/gflags.h"
#include "googletest/gtest/gtest.h"
#include "jsoncpp/json.h"

#include "flare/base/down_cast.h"
#include "flare/base/string.h"
#include "flare/rpc/binlog/tags.h"
#include "flare/rpc/binlog/testing.h"
#include "flare/rpc/binlog/text_only/dumper.h"

using namespace std::literals;

DECLARE_string(flare_binlog_dumper);
DECLARE_string(flare_binlog_text_only_dumper_filename);
DECLARE_int32(flare_binlog_dumper_sampling_interval);

namespace flare::binlog {

TEST(Dumper, All) {
  google::FlagSaver _;
  FLAGS_flare_binlog_dumper = "text_only";
  FLAGS_flare_binlog_text_only_dumper_filename = "./dump.txt";

  {
    auto log = GetDumper()->StartDumping();
    auto outgoing1 = log->StartOutgoingCall(),
         outgoing2 = log->StartOutgoingCall();

    outgoing1->SetSystemTags({{tags::kOperationName, "outgoing method"},
                              {tags::kUri, "http://my-fancy-uri:5678"}});
    outgoing1->SetOutgoingPackets(
        {NewIncomingPacket(outgoing1, TestingPacketDesc("outgoing_req"))});
    outgoing1->SetIncomingPackets(
        {NewIncomingPacket(outgoing1, TestingPacketDesc("outgoing_resp"))});

    outgoing2->SetSystemTags({{tags::kOperationName, "outgoing method"},
                              {tags::kUri, "http://my-fancy-uri:5678"}});
    outgoing1->SetOutgoingPackets(
        {NewIncomingPacket(outgoing1, TestingPacketDesc("outgoing_req"))});
    outgoing1->SetIncomingPackets(
        {NewIncomingPacket(outgoing1, TestingPacketDesc("outgoing_resp"))});

    auto&& incoming = log->GetIncomingCall();
    incoming->SetSystemTags({{tags::kServiceName, "incoming service"},
                             {tags::kOperationName, "incoming method"},
                             {tags::kLocalPeer, "192.0.2.1:5678"}});
    incoming->SetIncomingPackets(
        {NewIncomingPacket(incoming, TestingPacketDesc("incoming_req"))});
    incoming->SetOutgoingPackets(
        {NewIncomingPacket(incoming, TestingPacketDesc("incoming_resp"))});

    log->Dump();
  }

  Json::Value jsv;
  std::ifstream ifs("./dump.txt");
  ASSERT_TRUE(Json::Reader().parse(ifs, jsv));
  EXPECT_EQ("incoming service",
            jsv["incomingCall"]["systemTags"]["service_name"].asString());
  EXPECT_EQ("incoming method",
            jsv["incomingCall"]["systemTags"]["operation_name"].asString());
  EXPECT_EQ("outgoing method",
            jsv["outgoingCalls"][0]["systemTags"]["operation_name"].asString());
  EXPECT_EQ("http://my-fancy-uri:5678",
            jsv["outgoingCalls"][0]["systemTags"]["uri"].asString());
}

TEST(Dumper, AcquireSamplingQuotaForDumping) {
  google::FlagSaver _;
  FLAGS_flare_binlog_dumper = "text_only";
  FLAGS_flare_binlog_dumper_sampling_interval = 20;
  int sum = 0;
  auto start = ReadSteadyClock();
  while (ReadSteadyClock() - start < 1s) {
    sum += AcquireSamplingQuotaForDumping();
  }
  EXPECT_NEAR(sum, 1s / 20ms, 10);  // We're using coarse clock internally so
                                    // the error can be significant.
}

}  // namespace flare::binlog
