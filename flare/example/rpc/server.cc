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

#include "flare/rpc/server.h"

#include "flare/base/exposed_var.h"
#include "flare/base/random.h"
#include "flare/example/rpc/echo_service.flare.pb.h"
#include "flare/fiber/this_fiber.h"
#include "flare/init.h"
#include "flare/init/override_flag.h"
#include "flare/rpc/rpc_server_controller.h"

using namespace std::literals;

DEFINE_string(ip, "127.0.0.1", "IP address to listen on.");
DEFINE_int32(port, 5567, "Port to listen on.");
DEFINE_int32(fail_with, 0,
             "If set to non-zero, all requests are failed with this status.");

FLARE_OVERRIDE_FLAG(logbufsecs, 0);
FLARE_OVERRIDE_FLAG(logtostderr, true);
FLARE_OVERRIDE_FLAG(flare_fiber_scheduling_optimize_for, "io-heavy");

namespace example::protobuf_echo {

flare::ExposedCounter<std::size_t> counter("processed_reqs");
flare::ExposedMetrics<std::uint64_t> process_delay("process_delay");

class EchoServiceImpl : public SyncEchoService {
 public:
  void Echo(const EchoRequest& request, EchoResponse* response,
            flare::RpcServerController* ctlr) override {
    if (FLAGS_fail_with) {
      ctlr->SetFailed(FLAGS_fail_with, "Failed with configured error.");
    } else {
      ctlr->SetResponseAttachment(ctlr->GetRequestAttachment());
      response->set_body(request.body());
    }
    counter->Add(1);
    process_delay->Report(
        (flare::ReadSteadyClock() - ctlr->GetTimestampReceived()) / 1us);
  }
};

int Entry(int argc, char** argv) {
  flare::Server server{
      flare::Server::Options{.service_name = "example_echo_server"}};

  server.AddProtocols({"flare", "http+gdt-json", "http+pb", "http+proto3-json",
                       "qzone-pb", "baidu-std", "poppy"});
  server.AddService(std::make_unique<EchoServiceImpl>());
  server.ListenOn(flare::EndpointFromIpv4(FLAGS_ip, FLAGS_port));
  FLARE_CHECK(server.Start());

  auto last = counter->Read();
  while (!flare::CheckForQuitSignal()) {
    flare::this_fiber::SleepFor(1s);
    auto now = counter->Read();
    FLARE_LOG_INFO("Processed {} request(s) in 1 second.",
                   now - std::exchange(last, now));
  }
  server.Stop();
  server.Join();
  return 0;
}

}  // namespace example::protobuf_echo

int main(int argc, char** argv) {
  return flare::Start(argc, argv, example::protobuf_echo::Entry);
}
