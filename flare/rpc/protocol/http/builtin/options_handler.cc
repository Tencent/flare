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

#include "flare/rpc/protocol/http/builtin/options_handler.h"

#include <memory>
#include <string>

#include "thirdparty/gflags/gflags.h"
#include "thirdparty/jsoncpp/json.h"

#include "flare/base/option.h"
#include "flare/base/string.h"

FLARE_RPC_SERVER_REGISTER_BUILTIN_HTTP_PREFIX_HANDLER(
    [](auto&& owner) {
      return std::make_unique<flare::rpc::builtin::OptionsHandler>(
          "/inspect/options");
    },
    "/inspect/options");

namespace flare::rpc::builtin {

namespace {

Json::Value GetOptions(const std::string& path) {
  auto opts = option::DumpOptions();
  if (path.empty()) {
    return opts;
  }

  auto parts = Split(path, "/", true);
  auto ptr = &opts;
  for (auto&& e : parts) {
    // JsonCpp does not handle empty string well. Given that empty string is
    // invalid anyway, we raise an error early.
    if (e.empty()) {
      return Json::Value(Json::nullValue);
    }
    ptr = &(*ptr)[std::string(e)];
  }
  return *ptr;
}

}  // namespace

void OptionsHandler::OnGet(const HttpRequest& request, HttpResponse* response,
                           HttpServerContext* context) {
  FLARE_CHECK(StartsWith(request.uri(), uri_prefix_));
  auto path = request.uri().substr(uri_prefix_.size());
  if (!path.empty()) {
    FLARE_CHECK(path[0] == '/');  // Checked by regex used when registering this
                                  // handler.
    path.erase(path.begin());
  }
  auto opts = GetOptions(path);
  if (opts.isNull()) {
    GenerateDefaultResponsePage(HttpStatus::NotFound, response);
    return;
  }

  response->set_status(HttpStatus::OK);
  if (opts.isObject()) {
    response->headers()->Append("Content-Type", "application/json");
    response->set_body(Json::StyledWriter().write(opts));
  } else {
    response->headers()->Append("Content-Type", "text/plain");
    response->set_body(opts.asString());
  }
}

}  // namespace flare::rpc::builtin
