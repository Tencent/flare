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

#include "flare/rpc/protocol/http/builtin/exposed_vars_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "jsoncpp/json.h"

#include "flare/base/exposed_var.h"
#include "flare/base/string.h"

FLARE_RPC_SERVER_REGISTER_BUILTIN_HTTP_PREFIX_HANDLER(
    [](auto&& owner) {
      return std::make_unique<flare::rpc::builtin::ExposedVarsHandler>(
          "/inspect/vars");
    },
    "/inspect/vars");

namespace flare::rpc::builtin {

namespace {

// '.' / '..' is not handled by this method. They're valid node name in
// `ExposedVarGroup`.
void NormalizePath(std::string* path) {
  *path += "/";
  Replace("//", "/", path);
  CHECK_EQ(path->back(), '/');
  path->pop_back();
}

}  // namespace

ExposedVarsHandler::ExposedVarsHandler(std::string uri_prefix)
    : uri_prefix_(std::move(uri_prefix)) {}

void ExposedVarsHandler::OnGet(const HttpRequest& request,
                               HttpResponse* response,
                               HttpServerContext* context) {
  std::string abs_path;
  if (StartsWith(request.uri(),
                 uri_prefix_)) {  // Remove `uri_prefix_` from request URI.
    abs_path = request.uri().substr(uri_prefix_.size());
    NormalizePath(&abs_path);
    if (abs_path.empty()) {
      // FIXME: GCC12 -Werror=restrict
      abs_path = std::string("/");
    }
  }
  FLARE_CHECK(abs_path[0] == '/',
              "Unexpected: Requested URI [{}] is not an absolute path.",
              request.uri());

  auto opt_jsv = ExposedVarGroup::TryGet(abs_path);
  if (!opt_jsv) {
    GenerateDefaultResponsePage(HttpStatus::NotFound, response);
    return;
  }
  response->set_status(HttpStatus::OK);
  if (auto type = opt_jsv->type();
      type == Json::nullValue || type == Json::intValue ||
      type == Json::uintValue || type == Json::realValue ||
      type == Json::stringValue || type == Json::booleanValue) {
    response->headers()->Append("Content-Type", "text/plain");
    response->set_body(opt_jsv->asString());
  } else {
    response->headers()->Append("Content-Type", "application/json");
    response->set_body(Json::StyledWriter().write(*opt_jsv));
  }
}

}  // namespace flare::rpc::builtin
