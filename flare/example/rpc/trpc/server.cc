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
#include "flare/base/exposed_var.h"
#include "flare/example/rpc/trpc/greeter.flare.pb.h"
#include "flare/fiber/this_fiber.h"
#include "flare/init.h"
#include "flare/init/override_flag.h"

using namespace std::literals;

DEFINE_string(ip, "127.0.0.1", "IP address to listen on.");
DEFINE_int32(port, 5568, "Port to listen on.");

FLARE_OVERRIDE_FLAG(logtostderr, true);
FLARE_OVERRIDE_FLAG(flare_fiber_scheduling_optimize_for, "io-heavy");

namespace example::protobuf_echo {

flare::ExposedCounter<std::size_t> counter("processed_reqs");

class GreeterImpl : public trpc::test::helloworld::SyncGreeter {
 public:
  void SayHello(const trpc::test::helloworld::HelloRequest& request,
                trpc::test::helloworld::HelloReply* response,
                flare::RpcServerController* ctlr) override {
    response->set_msg(request.msg());
    counter->Add(1);
  }
};

int Entry(int argc, char** argv) {
  flare::Server server;

  server.AddProtocols({"trpc"});
  server.AddService(std::make_unique<GreeterImpl>());
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
