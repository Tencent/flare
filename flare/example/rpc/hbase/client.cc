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

#include "flare/example/rpc/hbase/echo_service.pb.h"
#include "flare/init.h"
#include "flare/init/override_flag.h"
#include "flare/net/hbase/hbase_channel.h"
#include "flare/net/hbase/hbase_client_controller.h"

using namespace std::literals;

DEFINE_string(echo_body, "Hello Word (tm).",
              "Message to send to echo service.");
DEFINE_string(server_addr, "hbase://127.0.0.1:60010",
              "When specified, the client uses address specified here, instead "
              "of reading it from config file.");
DEFINE_string(
    cell_block, "",
    "If set, its value is sent to server (and echoed back) as cell block.");

FLARE_OVERRIDE_FLAG(logtostderr, true);
FLARE_OVERRIDE_FLAG(flare_fiber_scheduling_optimize_for, "io-heavy");

namespace example::hbase_echo {

int Entry(int, char**) {
  flare::HbaseChannel::Options options = {
      .effective_user = "someone",
      .service_name = "example.hbase_echo.EchoService"};
  flare::HbaseChannel channel;
  FLARE_CHECK(channel.Open(FLAGS_server_addr, options));

  EchoService_Stub stub(&channel);
  EchoRequest req;
  EchoResponse resp;
  flare::HbaseClientController ctlr;
  req.set_body(FLAGS_echo_body);
  if (!FLAGS_cell_block.empty()) {
    ctlr.SetRequestCellBlock(flare::CreateBufferSlow(FLAGS_cell_block));
  }
  stub.Echo(&ctlr, &req, &resp, nullptr);

  FLARE_LOG_INFO("Succeeded: {}, Error Text: [{}].", !ctlr.Failed(),
                 ctlr.Failed() ? ctlr.ErrorText() : "");
  FLARE_LOG_INFO("Received: {}", resp.body());
  if (auto&& cb = ctlr.GetResponseCellBlock(); !cb.Empty()) {
    FLARE_LOG_INFO("Cell-block: {}", flare::FlattenSlow(cb));
  }
  return 0;
}

}  // namespace example::hbase_echo

int main(int argc, char** argv) {
  return flare::Start(argc, argv, example::hbase_echo::Entry);
}
