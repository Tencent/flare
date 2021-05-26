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

#ifndef FLARE_RPC_PROTOCOL_HTTP_BUILTIN_OPTIONS_HANDLER_H_
#define FLARE_RPC_PROTOCOL_HTTP_BUILTIN_OPTIONS_HANDLER_H_

#include <string>
#include <utility>

#include "flare/rpc/http_handler.h"

namespace flare::rpc::builtin {

// Handler of `/inspect/options`.
class OptionsHandler : public HttpHandler {
 public:
  explicit OptionsHandler(std::string prefix)
      : uri_prefix_(std::move(prefix)) {}

  // Get individual / all options.
  void OnGet(const HttpRequest& request, HttpResponse* response,
             HttpServerContext* context) override;

 private:
  std::string uri_prefix_;
};

}  // namespace flare::rpc::builtin

#endif  // FLARE_RPC_PROTOCOL_HTTP_BUILTIN_OPTIONS_HANDLER_H_
