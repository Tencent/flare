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

#include "flare/rpc/internal/rpc_metrics.h"

#include <chrono>
#include <thread>

#include "thirdparty/googletest/gtest/gtest.h"
#include "thirdparty/jsoncpp/json.h"

#include "flare/testing/echo_service.flare.pb.h"
#include "flare/testing/main.h"

namespace flare::rpc::detail {

TEST(RpcServiceStatsTest, Global) {
  const google::protobuf::ServiceDescriptor* service =
      testing::EchoService::descriptor();
  RpcMetrics::Instance()->Report(service->method(0), 0, 10, 1000, 100);
  RpcMetrics::Instance()->Report(service->method(0), 3, 20, 2000, 200);
  RpcMetrics::Instance()->Report(service->method(1), 0, 30, 3000, 300);

  Json::Value root;
  std::this_thread::sleep_for(std::chrono::seconds(2));
  RpcMetrics::Instance()->Dump(&root);
  std::cout << root;

  auto&& echo_stat = root[service->method(0)->full_name()];
  EXPECT_EQ(1, echo_stat["counter"]["failure"]["last_hour"].asUInt64());
  EXPECT_EQ(1, echo_stat["counter"]["failure"]["last_minute"].asUInt64());
  EXPECT_EQ(1, echo_stat["counter"]["failure"]["total"].asUInt64());
  EXPECT_EQ(1, echo_stat["counter"]["success"]["total"].asUInt64());
  EXPECT_EQ(2, echo_stat["counter"]["total"]["total"].asUInt64());
  EXPECT_EQ(15, echo_stat["latency"]["last_hour"]["average"].asUInt64());
  EXPECT_EQ(20, echo_stat["latency"]["last_hour"]["max"].asUInt64());
  EXPECT_EQ(10, echo_stat["latency"]["last_hour"]["min"].asUInt64());
  EXPECT_EQ(1500,
            echo_stat["packet_size_in"]["last_hour"]["average"].asUInt64());
  EXPECT_EQ(2000, echo_stat["packet_size_in"]["last_hour"]["max"].asUInt64());
  EXPECT_EQ(1000, echo_stat["packet_size_in"]["last_hour"]["min"].asUInt64());
  EXPECT_EQ(150,
            echo_stat["packet_size_out"]["last_hour"]["average"].asUInt64());
  EXPECT_EQ(200, echo_stat["packet_size_out"]["last_hour"]["max"].asUInt64());
  EXPECT_EQ(100, echo_stat["packet_size_out"]["last_hour"]["min"].asUInt64());

  auto&& echo_req_stat = root[service->method(1)->full_name()];
  EXPECT_EQ(1, echo_req_stat["counter"]["success"]["total"].asUInt64());
  EXPECT_EQ(1, echo_req_stat["counter"]["total"]["total"].asUInt64());
  EXPECT_EQ(30, echo_req_stat["latency"]["last_hour"]["average"].asUInt64());
  EXPECT_EQ(30, echo_req_stat["latency"]["last_minute"]["max"].asUInt64());
  EXPECT_EQ(30, echo_req_stat["latency"]["last_hour"]["min"].asUInt64());
  EXPECT_EQ(3000,
            echo_req_stat["packet_size_in"]["last_hour"]["average"].asUInt64());
  EXPECT_EQ(3000,
            echo_req_stat["packet_size_in"]["last_minute"]["max"].asUInt64());
  EXPECT_EQ(3000,
            echo_req_stat["packet_size_in"]["last_hour"]["min"].asUInt64());
  EXPECT_EQ(
      300, echo_req_stat["packet_size_out"]["last_hour"]["average"].asUInt64());
  EXPECT_EQ(300,
            echo_req_stat["packet_size_out"]["last_minute"]["max"].asUInt64());
  EXPECT_EQ(300,
            echo_req_stat["packet_size_out"]["last_hour"]["min"].asUInt64());

  auto&& global_stat = root["global"];
  EXPECT_EQ(1, global_stat["counter"]["failure"]["total"].asUInt64());
  EXPECT_EQ(2, global_stat["counter"]["success"]["total"].asUInt64());
  EXPECT_EQ(3, global_stat["counter"]["total"]["total"].asUInt64());
  EXPECT_EQ(20, global_stat["latency"]["last_hour"]["average"].asUInt64());
  EXPECT_EQ(30, global_stat["latency"]["last_minute"]["max"].asUInt64());
  EXPECT_EQ(10, global_stat["latency"]["last_minute"]["min"].asUInt64());
  EXPECT_EQ(2000,
            global_stat["packet_size_in"]["last_hour"]["average"].asUInt64());
  EXPECT_EQ(3000,
            global_stat["packet_size_in"]["last_minute"]["max"].asUInt64());
  EXPECT_EQ(1000,
            global_stat["packet_size_in"]["last_minute"]["min"].asUInt64());
  EXPECT_EQ(200,
            global_stat["packet_size_out"]["last_hour"]["average"].asUInt64());
  EXPECT_EQ(300,
            global_stat["packet_size_out"]["last_minute"]["max"].asUInt64());
  EXPECT_EQ(100,
            global_stat["packet_size_out"]["last_minute"]["min"].asUInt64());
}

}  // namespace flare::rpc::detail

FLARE_TEST_MAIN
