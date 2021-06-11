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

#ifndef FLARE_RPC_PROTOCOL_HTTP_BUILTIN_RPC_STATISTICS_HANDLER_H_
#define FLARE_RPC_PROTOCOL_HTTP_BUILTIN_RPC_STATISTICS_HANDLER_H_

#include "jsoncpp/value.h"

#include "flare/base/function.h"
#include "flare/rpc/http_handler.h"

namespace flare {

class Server;

}  // namespace flare

namespace flare::rpc::builtin {

// This class handles `/inspect/rpc_stats`.
//
// Note that statistics of all services hosted in this process are exported by
// this handler. We do not bind handler to a specific `Server` so that UDP
// services' statistics can also be exported via HTTP interfaces (otherwise UDP
// services' statistics cannot be exported as HTTP (except for HTTP/3) is
// running on TCP.).
//
// HACK: You should report your statistics to `LazyInit<gdt::RpcServiceStats>()`
// which is read by this class.
//
// TODO(luobogao): Once we implemented our own `RpcMetrics` class, we can simply
// make it a singleton and require all services to report everything to that
// singleton. This way we can remove this ugly hack.
class RpcStatisticsHandler : public HttpHandler {
 public:
  explicit RpcStatisticsHandler(Server* owner = nullptr) {}

  void OnGet(const HttpRequest& request, HttpResponse* response,
             HttpServerContext* context) override;
};

}  // namespace flare::rpc::builtin

#endif  // FLARE_RPC_PROTOCOL_HTTP_BUILTIN_RPC_STATISTICS_HANDLER_H_
