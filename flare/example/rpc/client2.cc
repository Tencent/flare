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

// A more powerful client.
//
// This one is used by Flare developer for internal testing purpose.

#include "flare/base/thread/latch.h"
#include "flare/example/rpc/echo_service.flare.pb.h"
#include "flare/example/rpc/relay_service.flare.pb.h"
#include "flare/fiber/async.h"
#include "flare/fiber/future.h"
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
DEFINE_int32(attachment_size, 0,
             "Size of attachment attached to echo request.");
DEFINE_string(server_addr, "flare://127.0.0.1:5567",
              "When specified, the client uses address specified here, instead "
              "of reading it from config file.");
DEFINE_string(override_nslb, "", "Override default NSLB for `server_addr`.");
DEFINE_int32(
    dummy_server_port, 0,
    "If set to non-zero, a dummy server is restart at the given port. This can "
    "help you in inspecting the framework actions (via /inspect/vars).");
DEFINE_bool(relay_stub, false,
            "If you're calling relay_server, enable this flag.");

FLARE_OVERRIDE_FLAG(logtostderr, true);

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

  flare::RpcChannel channel(
      FLAGS_server_addr,
      flare::RpcChannel::Options{.override_nslb = FLAGS_override_nslb});
  EchoService_SyncStub stub(&channel);
  EchoRequest req;
  flare::Expected<EchoResponse, flare::Status> resp;

  RelayService_SyncStub relay_stub(FLAGS_server_addr);
  RelayRequest relay_req;
  flare::Expected<RelayResponse, flare::Status> relay_resp;

  auto buffer =
      flare::CreateBufferSlow(std::string(FLAGS_attachment_size, 'a'));

  for (int i = 0; i < FLAGS_times; ++i) {
    flare::RpcClientController rpc_ctlr;
    std::string body;
    if (!FLAGS_body.empty()) {
      body = FLAGS_body;
    } else {
      body = std::string(FLAGS_body_size, 'A');
    }
    rpc_ctlr.SetRequestAttachment(buffer);

    auto start_ts = flare::ReadSteadyClock();
    if (!FLAGS_relay_stub) {
      req.set_body(body);
      resp = stub.Echo(req, &rpc_ctlr);
    } else {
      relay_req.set_body(body);
      relay_resp = relay_stub.Relay(relay_req, &rpc_ctlr);
    }

    if (rpc_ctlr.Failed()) {
      FLARE_LOG_INFO("Failed to call [{}].", FLAGS_server_addr);
    } else {
      auto resp_size =
          FLAGS_relay_stub ? relay_resp->body().size() : resp->body().size();
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
