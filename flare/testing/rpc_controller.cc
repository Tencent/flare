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

#include "flare/testing/rpc_controller.h"

#include <string>
#include <utility>

namespace flare::testing {

namespace detail {

void RpcControllerMaster::SetResponseAttachment(RpcClientController* ctlr,
                                                NoncontiguousBuffer buffer) {
  ctlr->SetResponseAttachment(std::move(buffer));
}

void RpcControllerMaster::SetRequestAttachment(RpcServerController* ctlr,
                                               NoncontiguousBuffer buffer) {
  ctlr->SetRequestAttachment(std::move(buffer));
}

void RpcControllerMaster::SetRequestRawBytes(RpcServerController* ctlr,
                                             NoncontiguousBuffer buffer) {
  ctlr->SetRequestRawBytes(std::move(buffer));
}

void RpcControllerMaster::SetResponseRawBytes(RpcClientController* ctlr,
                                              NoncontiguousBuffer buffer) {
  ctlr->SetResponseRawBytes(std::move(buffer));
}

void RpcControllerMaster::SetRemotePeer(RpcServerController* ctlr,
                                        const Endpoint& remote_peer) {
  ctlr->SetRemotePeer(remote_peer);
}

void RpcControllerMaster::SetTimeout(
    RpcServerController* ctlr, std::chrono::steady_clock::time_point timeout) {
  ctlr->SetTimeout(timeout);
}

void RpcControllerMaster::RunDone(RpcClientController* ctlr, rpc::Status status,
                                  std::string reason) {
  ctlr->NotifyCompletion(Status{status, reason});
}

}  // namespace detail

void SetRpcClientResponseAttachment(RpcClientController* ctlr,
                                    NoncontiguousBuffer buffer) {
  detail::RpcControllerMaster::SetResponseAttachment(ctlr, std::move(buffer));
}

void SetRpcServerRequestAttachment(RpcServerController* ctlr,
                                   NoncontiguousBuffer buffer) {
  detail::RpcControllerMaster::SetRequestAttachment(ctlr, std::move(buffer));
}

void SetRpcClientResponseRawBytes(RpcClientController* ctlr,
                                  NoncontiguousBuffer buffer) {
  detail::RpcControllerMaster::SetResponseRawBytes(ctlr, std::move(buffer));
}

void SetRpcServerRequestRawBytes(RpcServerController* ctlr,
                                 NoncontiguousBuffer buffer) {
  detail::RpcControllerMaster::SetRequestRawBytes(ctlr, std::move(buffer));
}

void SetRpcClientRunDone(RpcClientController* ctlr, rpc::Status status,
                         std::string reason) {
  detail::RpcControllerMaster::RunDone(ctlr, status, std::move(reason));
}

void SetRpcServerRemotePeer(RpcServerController* ctlr,
                            const Endpoint& remote_peer) {
  detail::RpcControllerMaster::SetRemotePeer(ctlr, remote_peer);
}

void SetRpcServerTimeout(RpcServerController* ctlr,
                         std::chrono::steady_clock::time_point timeout) {
  detail::RpcControllerMaster::SetTimeout(ctlr, timeout);
}

}  // namespace flare::testing
