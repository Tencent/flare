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

#include "thirdparty/gflags/gflags.h"

#include "flare/base/exposed_var.h"
#include "flare/base/monitoring.h"
#include "flare/example/rpc/echo_service.flare.pb.h"
#include "flare/example/rpc/relay_service.flare.pb.h"
#include "flare/fiber/this_fiber.h"
#include "flare/init.h"
#include "flare/init/override_flag.h"
#include "flare/rpc/rpc_channel.h"
#include "flare/rpc/rpc_client_controller.h"
#include "flare/rpc/rpc_server_controller.h"
#include "flare/rpc/server.h"

using namespace std::literals;

DEFINE_string(ip, "127.0.0.1", "IP address to listen on.");
DEFINE_int32(port, 5569, "Port to listen on.");
DEFINE_string(forward_to, "flare://127.0.0.1:5567",
              "Target IP to forward requests to.");
DEFINE_bool(
    always_succeed, false,
    "If set, even if the backend fails, the relay will report success.");

FLARE_OVERRIDE_FLAG(logbufsecs, 0);
FLARE_OVERRIDE_FLAG(logtostderr, true);
FLARE_OVERRIDE_FLAG(flare_fiber_scheduling_optimize_for, "io-heavy");

namespace example::protobuf_echo {

flare::ExposedCounter<std::size_t> counter("processed_reqs");
flare::MonitoredTimer attr1("gxt_flare_test");
flare::MonitoredTimer attr2("gxt_flare_test2", 1ms);

class RelayServiceImpl : public SyncRelayService {
 public:
  RelayServiceImpl() { FLARE_CHECK(channel_.Open(FLAGS_forward_to)); }

  void Relay(const RelayRequest& request, RelayResponse* response,
             flare::RpcServerController* svr_ctlr) override {
    flare::RpcClientController our_ctlr;
    EchoService_SyncStub stub(&channel_);
    EchoRequest echo_req;
    echo_req.set_body(request.body());
    our_ctlr.SetRequestAttachment(svr_ctlr->GetRequestAttachment());
    if (auto result = stub.Echo(echo_req, &our_ctlr)) {
      response->set_body(result->body());
      svr_ctlr->SetResponseAttachment(our_ctlr.GetResponseAttachment());
    } else {
      if (!FLAGS_always_succeed) {
        svr_ctlr->SetFailed(result.error().code(), result.error().message());
      }  // Nothing otherwise.
      svr_ctlr->SetTracingTag("backend error",
                              std::to_string(result.error().code()));
    }
    svr_ctlr->AddTracingLog("answer to universe: 42");
    svr_ctlr->SetTracingTag("another answer", 42);
    svr_ctlr->SetTracingTag("another answer + 1", 44);  // Oops wrong answer.
    if (!svr_ctlr->InDryRunEnvironment()) {
      svr_ctlr->SetBinlogTag("ctx", "my binlog context");
      svr_ctlr->SetBinlogTag("ctx-int-42", 42);
    }
    counter->Add(1);
    attr1.Report(svr_ctlr->GetElapsedTime());
    attr2.Report(svr_ctlr->GetElapsedTime(), {{"tag", "value"}});
  }

 private:
  flare::RpcChannel channel_;
};

int Entry(int argc, char** argv) {
  flare::Server server{
      flare::Server::Options{.service_name = "example_relay_server"}};

  server.AddProtocols(
      {"flare", "http+gdt-json", "http+pb", "qzone-pb", "trpc", "baidu-std"});
  server.AddService(std::make_unique<RelayServiceImpl>());
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
