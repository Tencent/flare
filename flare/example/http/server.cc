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
#include "flare/fiber/this_fiber.h"
#include "flare/init.h"
#include "flare/init/override_flag.h"
#include "flare/rpc/http_handler.h"

using namespace std::literals;

FLARE_OVERRIDE_FLAG(logtostderr, true);
FLARE_OVERRIDE_FLAG(flare_fiber_scheduling_optimize_for, "io-heavy");

namespace example::http_echo {

flare::ExposedCounter<std::size_t> counter("processed_reqs");

class EchoHandler : public flare::HttpHandler {
 public:
  void HandleRequest(const flare::HttpRequest& request,
                     flare::HttpResponse* response,
                     flare::HttpServerContext* context) override {
    response->set_status(flare::HttpStatus::OK);
    response->set_body(std::move(*request.body()));
    counter->Add(1);
  }
};

int Entry(int argc, char** argv) {
  flare::Server server;
  server.AddProtocol("http");
  server.AddHttpHandler("/path/to/echo.svc", std::make_unique<EchoHandler>());
  server.AddHttpHandler(
      "/path/to/echo-lambda.svc",
      flare::NewHttpGetHandler([](auto&& r, auto&& w, auto&& c) {
        w->set_status(flare::HttpStatus::OK);
        w->set_body("Echo from a fancy lambda: " + *r.body());
        counter->Add(1);
      }));
  server.ListenOn(flare::EndpointFromIpv4("0.0.0.0", 8888));
  FLARE_CHECK(server.Start());

  while (!flare::CheckForQuitSignal()) {
    auto last = counter->Read();
    flare::this_fiber::SleepFor(1s);
    FLARE_LOG_INFO("Processed {} request(s) in 1 second.",
                   counter->Read() - last);
  }
  server.Stop();
  server.Join();
  return 0;
}

}  // namespace example::http_echo

int main(int argc, char** argv) {
  return flare::Start(argc, argv, example::http_echo::Entry);
}
