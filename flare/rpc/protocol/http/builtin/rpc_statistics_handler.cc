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

#include "flare/rpc/protocol/http/builtin/rpc_statistics_handler.h"

#include <mutex>
#include <string>

#include "jsoncpp/json.h"

#include "flare/base/string.h"
#include "flare/rpc/internal/rpc_metrics.h"

FLARE_RPC_SERVER_REGISTER_BUILTIN_HTTP_PREFIX_HANDLER(
    [](auto&& owner) {
      return std::make_unique<flare::rpc::builtin::RpcStatisticsHandler>();
    },
    "/inspect/rpc_stats");

using namespace std::literals;

namespace flare::rpc::builtin {

namespace {

Json::Value TraverseJsonTree(const Json::Value& value,
                             const std::string& path) {
  auto splitted = Split(path, "/");
  Json::Value current = value;

  for (auto&& key : splitted) {
    current = current[std::string(key)];
  }
  return current;
}

}  // namespace

void RpcStatisticsHandler::OnGet(const HttpRequest& request,
                                 HttpResponse* response,
                                 HttpServerContext* context) {
  constexpr auto kPrefix = "/inspect/rpc_stats"sv;

  // The frameworks shouldn't call us otherwise.
  FLARE_CHECK(StartsWith(request.uri(), kPrefix));
  Json::Value jsv;
  detail::RpcMetrics::Instance()->Dump(&jsv);

  auto path = request.uri().substr(kPrefix.size());
  if (!path.empty() && path.front() != '/') {
    GenerateDefaultResponsePage(HttpStatus::NotFound, response);
    return;
  }

  auto body = TraverseJsonTree(jsv, path);
  if (body.isNull()) {
    GenerateDefaultResponsePage(HttpStatus::NotFound, response);
    return;
  }

  response->set_status(HttpStatus::OK);
  response->headers()->Append("Content-Type", "application/json");
  response->set_body(Json::StyledWriter().write(body));
}

}  // namespace flare::rpc::builtin
