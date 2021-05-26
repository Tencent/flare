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

#include "flare/example/rpc/mixed_echo/echo_service.flare.pb.h"
#include "flare/init.h"
#include "flare/init/override_flag.h"
#include "flare/rpc/http_handler.h"
#include "flare/rpc/server.h"

using namespace std::literals;

FLARE_OVERRIDE_FLAG(flare_fiber_scheduling_optimize_for, "io-heavy");

namespace example::mixed_echo {

class EchoServiceImpl : public SyncEchoService {
 public:
  void Echo(const EchoRequest& request, EchoResponse* response,
            flare::RpcServerController* controller) override {
    response->set_body(
        "No, strictly speaking I'm not echoing. Nonetheless, you sent: '" +
        request.body() + "'.");
  }
};

class EchoHandler : public flare::HttpHandler {
 public:
  void HandleRequest(const flare::HttpRequest& request,
                     flare::HttpResponse* response,
                     flare::HttpServerContext* context) override {
    response->set_status(flare::HttpStatus::OK);
    response->set_body("This is indeed an echo service. Echoing: " +
                       *request.body());
  }
};

int Entry(int argc, char** argv) {
  flare::Server server;

  server.AddProtocol("http");
  server.AddProtocol("http+proto3-json");
  server.AddService(std::make_unique<EchoServiceImpl>());
  server.AddHttpHandler("/path/to/echo.svc", std::make_unique<EchoHandler>());
  server.ListenOn(flare::EndpointFromIpv4("127.0.0.1", 8765));
  FLARE_CHECK(server.Start());

  flare::WaitForQuitSignal();
  server.Stop();
  server.Join();
  return 0;
}

}  // namespace example::mixed_echo

int main(int argc, char** argv) {
  return flare::Start(argc, argv, example::mixed_echo::Entry);
}
