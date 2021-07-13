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

#include "flare/rpc/protocol/http/builtin/gflags_handler.h"

#include <string>
#include <vector>

#include "gflags/gflags.h"
#include "jsoncpp/json.h"

#include "flare/base/string.h"

FLARE_RPC_SERVER_REGISTER_BUILTIN_HTTP_HANDLER(
    flare::rpc::builtin::GflagsHandler, "/inspect/gflags");

namespace flare::rpc::builtin {
namespace {

bool IsFlagHidden(const google::CommandLineFlagInfo& flag) {
  // Exactly the same logic as `common/`.
  return EndsWith(flag.filename, "/gflags_reporting.cc") ||
         EndsWith(flag.filename, "/gflags_completions.cc");
}

Json::Value GetFlags(const std::vector<std::string_view>& keys) {
  // Dump flags requested first.
  std::vector<google::CommandLineFlagInfo> flags;
  if (keys.empty()) {
    google::GetAllFlags(&flags);
  } else {
    for (auto&& k : keys) {
      google::GetCommandLineFlagInfo(std::string(k).c_str(),
                                     &flags.emplace_back());
    }
  }

  // Translate it to JSON then.
  Json::Value jsv;
  for (auto&& f : flags) {
    if (!IsFlagHidden(f)) {
      auto&& entry = jsv[f.name];
      entry["type"] = f.type;
      entry["filename"] = f.filename;
      entry["default_value"] = f.default_value;
      entry["current_value"] = f.current_value;
      entry["is_default"] = f.is_default;
      entry["description"] = f.description;
    }
  }
  return jsv;
}

// TODO(luobogao): We should use a dedicated class to parse query string.
bool ParseFlagNames(std::string_view uri,
                    std::vector<std::string_view>* flags) {
  flags->clear();
  auto pos = uri.find('?');
  if (pos == std::string_view::npos) {
    return true;
  }
  auto query_str = uri.substr(pos + 1, uri.find('#', pos));
  auto queries = Split(query_str, "&");
  for (auto&& e : queries) {
    auto pos = e.find('=');
    if (pos == std::string_view::npos) {
      return false;
    }
    auto key = e.substr(0, pos);
    auto value = e.substr(pos + 1);
    if (key == "name") {
      *flags = Split(value, ",");
      return true;
    }
  }
  return true;
}

}  // namespace

void GflagsHandler::OnGet(const HttpRequest& request, HttpResponse* response,
                          HttpServerContext* context) {
  std::vector<std::string_view> keys;
  if (!ParseFlagNames(request.uri(), &keys)) {
    GenerateDefaultResponsePage(HttpStatus::BadRequest, response);
    return;
  }

  // For the moment we can only return JSON.
  //
  // It would be better if HTML is rendered separately using data returned
  // from this interface.
  //
  // We don't check `Accept-Type` here, returning an HTTP 501 is not any
  // better than just returning plain JSON.

  response->set_status(HttpStatus::OK);
  response->headers()->Append("Content-Type", "application/json");
  response->set_body(Json::StyledWriter().write(GetFlags(keys)));
}

void GflagsHandler::OnPost(const HttpRequest& request, HttpResponse* response,
                           HttpServerContext* context) {
  Json::Value json_req;
  if (!Json::Reader().parse(*request.body(), json_req)) {
    GenerateDefaultResponsePage(HttpStatus::BadRequest, response);
    return;
  }

  Json::Value failures;
  if (!json_req.empty()) {
    for (auto iter = json_req.begin(); iter != json_req.end(); ++iter) {
      auto&& key = iter.key().asString();
      auto&& value = json_req[key].asString();
      auto rc = google::SetCommandLineOption(key.c_str(), value.c_str());
      if (rc.empty()) {
        failures[key] = rc;
      } else {  // Success.
        if (rc != value) {
          FLARE_LOG_INFO("Flag [{}] is set to [{}], but [{}] was intended.",
                         key, rc, value);
        } else {
          FLARE_LOG_INFO("Flag [{}] is set to [{}].", key, rc, value);
        }
      }
    }
  }

  // FIXME: What if all set operation were failed?
  response->set_status(HttpStatus::OK);
  response->headers()->Append("Content-Type", "application/json");
  if (!failures.empty()) {
    response->set_body(Json::StyledWriter().write(failures));
  }
}

}  // namespace flare::rpc::builtin
