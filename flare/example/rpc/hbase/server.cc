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

#include "flare/rpc/server.h"
#include "flare/base/down_cast.h"
#include "flare/example/rpc/hbase/echo_service.pb.h"
#include "flare/fiber/this_fiber.h"
#include "flare/init.h"
#include "flare/init/override_flag.h"
#include "flare/net/hbase/hbase_server_controller.h"
#include "flare/net/hbase/hbase_service.h"

using namespace std::literals;

DEFINE_string(ip, "127.0.0.1", "IP address to listen on.");
DEFINE_int32(port, 60010, "Port to listen on.");

FLARE_OVERRIDE_FLAG(logtostderr, true);
FLARE_OVERRIDE_FLAG(flare_fiber_scheduling_optimize_for, "io-heavy");

namespace example::hbase_echo {

class EchoServiceImpl : public EchoService {
 public:
  void Echo(google::protobuf::RpcController* controller,
            const EchoRequest* request, EchoResponse* response,
            google::protobuf::Closure* done) override {
    auto ctlr = flare::down_cast<flare::HbaseServerController>(controller);
    if (!ctlr->GetRequestCellBlock().Empty()) {
      ctlr->SetResponseCellBlock(flare::CreateBufferSlow(
          "Echoing: " + flare::FlattenSlow(ctlr->GetRequestCellBlock())));
    }
    response->set_body(request->body());
    done->Run();
  }
};

int Entry(int argc, char** argv) {
  EchoServiceImpl service_impl;
  flare::Server server;

  server.AddProtocol("hbase");
  server.GetBuiltinNativeService<flare::HbaseService>()->AddService(
      &service_impl);
  server.ListenOn(flare::EndpointFromIpv4(FLAGS_ip, FLAGS_port));
  FLARE_LOG_FATAL_IF(!server.Start(), "Cannot start server.");

  flare::WaitForQuitSignal();
  server.Stop();
  server.Join();
  return 0;
}

}  // namespace example::hbase_echo

int main(int argc, char** argv) {
  return flare::Start(argc, argv, example::hbase_echo::Entry);
}
