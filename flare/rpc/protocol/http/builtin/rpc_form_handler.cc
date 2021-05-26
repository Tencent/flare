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

#include "flare/rpc/protocol/http/builtin/rpc_form_handler.h"

#include <memory>
#include <string>

#include "thirdparty/ctemplate/template.h"
#include "thirdparty/ctemplate/template_dictionary.h"

#include "flare/base/string.h"
#include "flare/rpc/protocol/http/builtin/static_resources.h"

FLARE_RPC_SERVER_REGISTER_BUILTIN_HTTP_PREFIX_HANDLER(
    [](auto&& owner) {
      return std::make_unique<flare::rpc::builtin::RpcFormHandler>();
    },
    "/inspect/rpc");

namespace flare::rpc::builtin {

void RpcFormHandler::OnGet(const HttpRequest& request, HttpResponse* response,
                           HttpServerContext* context) {
  std::string method_name = request.uri();
  const std::string kRpcFormPrefix = "/inspect/rpc/";
  CHECK_EQ(method_name.find(kRpcFormPrefix), 0);
  method_name = method_name.substr(kRpcFormPrefix.size());

  std::string html_template_filename = "form.html";
  auto&& html_template =
      RESOURCE_flare_rpc_protocol_http_builtin_resources_template_form_html;
  ctemplate::TemplateDictionary dict("data");
  dict.SetValue("METHOD_FULL_NAME", method_name);

  ctemplate::StringToTemplateCache(html_template_filename, html_template,
                                   sizeof(html_template),
                                   ctemplate::STRIP_WHITESPACE);
  ctemplate::ExpandTemplate(html_template_filename, ctemplate::STRIP_WHITESPACE,
                            &dict, response->body());
  response->headers()->Append("Content-Type", "text/html; charset=utf-8");
  response->set_status(HttpStatus::OK);
}

}  // namespace flare::rpc::builtin
