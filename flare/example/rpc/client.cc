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

// A simple client. Provided for illustration purpose.

#include "gflags/gflags.h"

#include "flare/example/rpc/echo_service.flare.pb.h"
#include "flare/init.h"
#include "flare/init/override_flag.h"
#include "flare/rpc/rpc_channel.h"
#include "flare/rpc/rpc_client_controller.h"

using namespace std::literals;

DEFINE_string(body, "Hello there.", "Message to send to echo service.");
DEFINE_string(server_addr, "flare://127.0.0.1:5567",
              "When specified, the client uses address specified here, instead "
              "of reading it from config file.");

FLARE_OVERRIDE_FLAG(logtostderr, true);

namespace example::protobuf_echo {

int Entry(int, char**) {
  EchoService_SyncStub stub(FLAGS_server_addr);
  EchoRequest req;
  req.set_body(FLAGS_body);

  flare::RpcClientController ctlr;
  if (auto resp = stub.Echo(req, &ctlr)) {
    FLARE_LOG_INFO("Received: [{}]", resp->body());
  } else {
    FLARE_LOG_INFO("Failed to call [{}].", FLAGS_server_addr);
  }
  return 0;
}

}  // namespace example::protobuf_echo

int main(int argc, char** argv) {
  return flare::Start(argc, argv, example::protobuf_echo::Entry);
}
