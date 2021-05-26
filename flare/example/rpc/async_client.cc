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

#include "flare/example/rpc/echo_service.flare.pb.h"
#include "flare/init.h"
#include "flare/init/override_flag.h"
#include "flare/rpc/rpc_channel.h"
#include "flare/rpc/rpc_client_controller.h"
#include "flare/rpc/server.h"
#include "flare/rpc/server_group.h"

using namespace std::literals;

DEFINE_string(body, "", "Message to send to echo service.");
DEFINE_int32(
    body_size, 0,
    "Size of body of echo request. Not applicable if `body` is not empty.");
DEFINE_int32(times, 1, "Number of times `Echo` is called.");
DEFINE_string(server_addr, "flare://127.0.0.1:5567",
              "When specified, the client uses address specified here, instead "
              "of reading it from config file.");
DEFINE_int32(
    dummy_server_port, 0,
    "If set to non-zero, a dummy server is restart at the given port. This can "
    "help you in inspecting the framework actions (via /inspect/vars).");

FLARE_OVERRIDE_FLAG(flare_fiber_scheduling_optimize_for, "io-heavy");

namespace example::protobuf_echo {

int Entry(int, char**) {
  flare::ServerGroup servers;

  if (FLAGS_dummy_server_port) {
    auto&& server = *servers.AddServer();
    server.ListenOn(
        flare::EndpointFromIpv4("127.0.0.1", FLAGS_dummy_server_port));
    server.AddProtocol("http");
  }
  servers.Start();

  flare::RpcChannel channel;
  FLARE_CHECK(channel.Open(FLAGS_server_addr));

  EchoService_AsyncStub stub(&channel);
  EchoRequest req;
  EchoResponse resp;

  for (int i = 0; i < FLAGS_times; ++i) {
    flare::RpcClientController rpc_ctlr;
    std::string body;
    if (!FLAGS_body.empty()) {
      body = FLAGS_body;
    } else {
      body = std::string(FLAGS_body_size, 'A');
    }

    auto start_ts = flare::ReadSteadyClock();
    req.set_body(body);
    auto&& opt_resp = flare::fiber::BlockingGet(stub.Echo(req, &rpc_ctlr));

    if (!opt_resp) {
      FLARE_LOG_INFO("Failed to call [{}]: {}", opt_resp.error().ToString());
    } else {
      auto resp_size = opt_resp->body().size();
      FLARE_LOG_INFO(
          "Received: {} bytes, time elapsed: {}, I/O time-cost: {}", resp_size,
          (flare::ReadSteadyClock() - start_ts) / 1us,
          (rpc_ctlr.GetTimestampReceived() - rpc_ctlr.GetTimestampSent()) /
              1us);
    }
  }
  return 0;
}

}  // namespace example::protobuf_echo

int main(int argc, char** argv) {
  return flare::Start(argc, argv, example::protobuf_echo::Entry);
}
