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

#ifndef FLARE_RPC_INTERNAL_DRY_RUN_CONNECTION_HANDLER_H_
#define FLARE_RPC_INTERNAL_DRY_RUN_CONNECTION_HANDLER_H_

#include <memory>
#include <vector>

#include "flare/base/net/endpoint.h"
#include "flare/rpc/internal/server_connection_handler.h"

namespace flare {

class Server;
class StreamService;

}  // namespace flare

namespace flare::binlog {

class DryRunContext;
class LogReader;

}  // namespace flare::binlog

namespace flare::rpc::detail {

class DryRunConnectionHandler : public ServerConnectionHandler {
 public:
  struct Context {
    std::uint64_t id;
    Endpoint local_peer;
    Endpoint remote_peer;
    std::vector<StreamService*> services;
  };

  DryRunConnectionHandler(Server* owner, std::unique_ptr<Context> ctx);

  void Stop() override;
  void Join() override;

 private:
  // We do not care about these events.
  void OnAttach(StreamConnection* conn) override;
  void OnDetach() override;
  void OnWriteBufferEmpty() override;
  void OnDataWritten(std::uintptr_t ctx) override;

  DataConsumptionStatus OnDataArrival(NoncontiguousBuffer* buffer) override;

  void OnClose() override;
  void OnError() override;

 private:
  bool StartNewCall();
  void FinishCall();

  void ProcessOneDryRunContext(
      std::unique_ptr<binlog::DryRunContext> dry_run_ctx);
  void ServiceDryRunFor(std::unique_ptr<binlog::LogReader> dry_run_ctx,
                        StreamService* handler);

 private:
  Server* owner_;
  std::unique_ptr<Context> ctx_;
  StreamConnection* conn_;

  // Unfinished calls to services.
  std::atomic<std::size_t> ongoing_requests_{0};
};

}  // namespace flare::rpc::detail

#endif  // FLARE_RPC_INTERNAL_DRY_RUN_CONNECTION_HANDLER_H_
