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

#include "flare/rpc/protocol/http/builtin/static_resource_http_handler.h"

#include <memory>
#include <string>

#include "flare/rpc/protocol/http/builtin/static_resources.h"

namespace flare::rpc::builtin {

#define FLARE_RPC_REGISTER_BUILTIN_STATIC_RESOURCE_HTTP_HANDLER( \
    Content, ContentType, ...)                                   \
  FLARE_ON_INIT(0, [] {                                          \
    ::flare::detail::RegisterBuiltinHttpHandlerFactory(          \
        [](auto&& owner) {                                       \
          return std::make_unique<                               \
              ::flare::rpc::builtin::StaticResourceHttpHandler>( \
              owner, Content, sizeof(Content), ContentType);     \
        },                                                       \
        {__VA_ARGS__});                                          \
  })

FLARE_RPC_REGISTER_BUILTIN_STATIC_RESOURCE_HTTP_HANDLER(
    RESOURCE_flare_rpc_protocol_http_builtin_resources_static_forms_html,
    "text/html; charset=utf-8", "/inspect/rpc");

FLARE_RPC_REGISTER_BUILTIN_STATIC_RESOURCE_HTTP_HANDLER(
    RESOURCE_flare_rpc_protocol_http_builtin_resources_static_forms_js, "",
    "/inspect/static/forms.js");

FLARE_RPC_REGISTER_BUILTIN_STATIC_RESOURCE_HTTP_HANDLER(
    RESOURCE_flare_rpc_protocol_http_builtin_resources_static_jquery_json_2_2_min_js,
    "", "/inspect/static/jquery.json-2.2.min.js");

FLARE_RPC_REGISTER_BUILTIN_STATIC_RESOURCE_HTTP_HANDLER(
    RESOURCE_flare_rpc_protocol_http_builtin_resources_static_jquery_1_11_2_min_js,
    "", "/inspect/static/jquery-1.11.2.min.js");

FLARE_RPC_REGISTER_BUILTIN_STATIC_RESOURCE_HTTP_HANDLER(
    RESOURCE_flare_rpc_protocol_http_builtin_resources_static_codemirror_lib_codemirror_css,
    "", "/inspect/static/codemirror/lib/codemirror.css");

FLARE_RPC_REGISTER_BUILTIN_STATIC_RESOURCE_HTTP_HANDLER(
    RESOURCE_flare_rpc_protocol_http_builtin_resources_static_codemirror_lib_codemirror_js,
    "", "/inspect/static/codemirror/lib/codemirror.js");

FLARE_RPC_REGISTER_BUILTIN_STATIC_RESOURCE_HTTP_HANDLER(
    RESOURCE_flare_rpc_protocol_http_builtin_resources_static_codemirror_mode_javascript_javascript_js,
    "", "/inspect/static/codemirror/mode/javascript/javascript.js");

FLARE_RPC_REGISTER_BUILTIN_STATIC_RESOURCE_HTTP_HANDLER(
    RESOURCE_flare_rpc_protocol_http_builtin_resources_static_antd_4_8_2_css,
    "", "/inspect/static/antd-4.8.2.css");

StaticResourceHttpHandler::StaticResourceHttpHandler(
    Server* owner, const char* content, std::size_t content_length,
    const std::string& content_type)
    : content_(content),
      content_length_(content_length),
      content_type_(content_type) {
  if (!content_type.empty()) {
    http_headers_.Append("Content-Type", content_type);
  }

  http_headers_.Append("Cache-Control", "max-age=60");
}

void StaticResourceHttpHandler::OnGet(const HttpRequest& request,
                                      HttpResponse* response,
                                      HttpServerContext* context) {
  response->set_status(HttpStatus::OK);
  *response->headers() = http_headers_;
  response->body()->assign(content_, content_length_);
}

}  // namespace flare::rpc::builtin
