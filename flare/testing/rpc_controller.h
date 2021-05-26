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

#ifndef FLARE_TESTING_RPC_CONTROLLER_H_
#define FLARE_TESTING_RPC_CONTROLLER_H_

// When writing UTs, sometimes you might want to access several private fields
// in `RpcXxxController`. Here we provide some helper methods for you to do
// these.

#include <string>

#include "flare/rpc/rpc_client_controller.h"
#include "flare/rpc/rpc_server_controller.h"

namespace flare::testing {

namespace detail {

struct RpcControllerMaster {
  static void SetResponseAttachment(RpcClientController* ctlr,
                                    NoncontiguousBuffer buffer);

  static void SetRequestAttachment(RpcServerController* ctlr,
                                   NoncontiguousBuffer buffer);

  static void SetRequestRawBytes(RpcServerController* ctlr,
                                 NoncontiguousBuffer buffer);

  static void SetResponseRawBytes(RpcClientController* ctlr,
                                  NoncontiguousBuffer buffer);

  static void SetRemotePeer(RpcServerController* ctlr,
                            const Endpoint& remote_peer);

  static void SetTimeout(RpcServerController* ctlr,
                         const std::chrono::steady_clock::time_point& timeout);

  static void RunDone(RpcClientController* ctlr, rpc::Status status,
                      std::string reason);
};

}  // namespace detail

void SetRpcClientResponseAttachment(RpcClientController* ctlr,
                                    NoncontiguousBuffer buffer);
void SetRpcServerRequestAttachment(RpcServerController* ctlr,
                                   NoncontiguousBuffer buffer);
void SetRpcClientResponseRawBytes(RpcClientController* ctlr,
                                  NoncontiguousBuffer buffer);
void SetRpcServerRequestRawBytes(RpcServerController* ctlr,
                                 NoncontiguousBuffer buffer);
void SetRpcClientRunDone(RpcClientController* ctlr, rpc::Status status,
                         std::string reason);
void SetRpcServerRemotePeer(RpcServerController* ctlr,
                            const Endpoint& remote_peer);
void SetRpcServerTimeout(RpcServerController* ctlr,
                         const std::chrono::steady_clock::time_point& timeout);

}  // namespace flare::testing

#endif  // FLARE_TESTING_RPC_CONTROLLER_H_
