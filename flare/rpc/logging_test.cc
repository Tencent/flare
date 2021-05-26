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

#include "flare/rpc/logging.h"

#include <memory>
#include <string>
#include <vector>

#include "thirdparty/googletest/gmock/gmock-matchers.h"
#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/rpc/rpc_channel.h"
#include "flare/rpc/rpc_client_controller.h"
#include "flare/rpc/server_group.h"
#include "flare/testing/echo_service.flare.pb.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"

namespace flare::fiber {

struct AwesomeLogSink : public google::LogSink {
  void send(google::LogSeverity severity, const char* full_filename,
            const char* base_filename, int line, const struct ::tm* tm_time,
            const char* message, size_t message_len) override {
    msgs.emplace_back(message, message_len);
  }
  std::vector<std::string> msgs;
};

class EchoServiceImpl : public testing::SyncEchoService {
 public:
  void Echo(const testing::EchoRequest& request,
            testing::EchoResponse* response,
            flare::RpcServerController* ctlr) override {
    AddLoggingTagToRpc("crash_id", request.body());  // String.
    FLARE_LOG_INFO("crashing.");
    FLARE_LOG_WARNING("crashing..");
    FLARE_LOG_ERROR("crashing...");

    AddLoggingItemToRpc(Format("crash_id 2: {}4", request.body()));  // KV-pair.
    FLARE_LOG_INFO("crashing.");
    FLARE_LOG_WARNING("crashing..");
    FLARE_LOG_ERROR("crashing...");
  }
};

TEST(Logging, Prefix) {
  AwesomeLogSink sink;
  google::AddLogSink(&sink);

  auto ep = testing::PickAvailableEndpoint();
  ServerGroup server_group;
  server_group.AddServer(ep, {"flare"}, std::make_unique<EchoServiceImpl>());
  server_group.Start();

  RpcChannel channel;
  channel.Open("flare://" + ep.ToString());
  testing::EchoService_SyncStub stub(&channel);
  RpcClientController ctlr;
  testing::EchoRequest req;
  req.set_body("body123");
  ASSERT_TRUE(stub.Echo(req, &ctlr));

  auto uninterested_log = [](auto&& e) {
    // Not printed by us then.
    return e.find("crashing.") == std::string::npos;
  };
  sink.msgs.erase(
      std::remove_if(sink.msgs.begin(), sink.msgs.end(), uninterested_log),
      sink.msgs.end());

  ASSERT_THAT(
      sink.msgs,
      ::testing::ElementsAre(
          "[crash_id: body123] crashing.", "[crash_id: body123] crashing..",
          "[crash_id: body123] crashing...",
          "[crash_id: body123] [crash_id 2: body1234] crashing.",
          "[crash_id: body123] [crash_id 2: body1234] crashing..",
          "[crash_id: body123] [crash_id 2: body1234] crashing..."));

  google::RemoveLogSink(&sink);
}

}  // namespace flare::fiber

FLARE_TEST_MAIN
