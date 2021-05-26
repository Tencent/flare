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

#include "flare/example/custom_protocol/protocol.h"
#include "flare/example/custom_protocol/service.h"
#include "flare/init.h"
#include "flare/rpc/server.h"

using namespace std::literals;

namespace example::naive_proto {

int Entry(int argc, char** argv) {
  flare::Server server;
  Service svc;

  server.AddProtocol(&std::make_unique<Protocol>);
  server.AddNativeService(flare::MaybeOwning(flare::non_owning, &svc));
  server.ListenOn(flare::EndpointFromIpv4("127.0.0.1", 5566));
  FLARE_CHECK(server.Start());

  flare::WaitForQuitSignal();
  server.Stop();
  server.Join();
  return 0;
}

}  // namespace example::naive_proto

int main(int argc, char** argv) {
  return flare::Start(argc, argv, example::naive_proto::Entry);
}
