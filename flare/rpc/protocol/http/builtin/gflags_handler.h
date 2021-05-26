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

#ifndef FLARE_RPC_PROTOCOL_HTTP_BUILTIN_GFLAGS_HANDLER_H_
#define FLARE_RPC_PROTOCOL_HTTP_BUILTIN_GFLAGS_HANDLER_H_

#include "flare/rpc/http_handler.h"

namespace flare {

class Server;

}  // namespace flare

namespace flare::rpc::builtin {

// Handler of `/inspect/gflags`. Automatically registered by `Server`.
class GflagsHandler : public HttpHandler {
 public:
  explicit GflagsHandler(Server* owner = nullptr) {}

  // Get individual / all flags.
  void OnGet(const HttpRequest& request, HttpResponse* response,
             HttpServerContext* context) override;

  // Set flag to specified value.
  void OnPost(const HttpRequest& request, HttpResponse* response,
              HttpServerContext* context) override;
};

}  // namespace flare::rpc::builtin

#endif  // FLARE_RPC_PROTOCOL_HTTP_BUILTIN_GFLAGS_HANDLER_H_
