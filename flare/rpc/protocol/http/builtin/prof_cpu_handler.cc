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

#include "flare/rpc/protocol/http/builtin/prof_cpu_handler.h"

#include <dlfcn.h>
#include <fcntl.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "glog/logging.h"
#include "jsoncpp/json.h"

#include "flare/base/string.h"
#include "flare/rpc/builtin/detail/prof_utility.h"

extern "C" {

int ProfilerStart(const char* fname);
void ProfilerFlush();
void ProfilerStop();

}  // extern "C"

FLARE_RPC_SERVER_REGISTER_BUILTIN_HTTP_PREFIX_HANDLER(
    [](auto&& owner) {
      return std::make_unique<flare::rpc::builtin::ProfCpuHandler>("/prof/cpu");
    },
    "/prof/cpu");

using namespace std::literals;

namespace flare::rpc::builtin {

static constexpr auto kTmpProfileFileName = "profiler.prof";

ProfCpuHandler::ProfCpuHandler(std::string uri_prefix)
    : uri_prefix_(std::move(uri_prefix)) {
#define REGISTER_PPROF_PATH_HANDLER(path, handler)                  \
  prof_path_handler_[path] = [this](auto&& r, auto&& w, auto&& c) { \
    handler(r, w, c);                                               \
  };
  REGISTER_PPROF_PATH_HANDLER("/start", DoStart);
  REGISTER_PPROF_PATH_HANDLER("/view", DoView);
  REGISTER_PPROF_PATH_HANDLER("/stop", DoStop);

  proc_path_ = ReadProcPath();
}

void ProfCpuHandler::OnGet(const HttpRequest& request, HttpResponse* response,
                           HttpServerContext* context) {
  std::string path;
  if (StartsWith(request.uri(), uri_prefix_)) {
    path = request.uri().substr(uri_prefix_.size());
    if (path.back() == '/') {
      path.pop_back();
    }
  }

  if (prof_path_handler_.find(path) != prof_path_handler_.end()) {
    prof_path_handler_[path](request, response, context);
    return;
  }
  response->set_status(HttpStatus::BadRequest);
  SetBodyWithCode(response->body(), ResponseErrorCode::ErrorPath);
}

void ProfCpuHandler::DoStart(const HttpRequest& request, HttpResponse* response,
                             HttpServerContext* context) {
  std::scoped_lock _(profile_lock_);
  if (running) {
    response->set_status(HttpStatus::BadRequest);
    SetBodyWithCode(response->body(), ResponseErrorCode::ErrorAlreadyStart);
    return;
  }
  if (ProfilerStart(kTmpProfileFileName)) {
    running = true;
    response->set_status(HttpStatus::OK);
    SetBodyWithCode(response->body(), ResponseErrorCode::Succ);
  } else {
    response->set_status(HttpStatus::InternalServerError);
    SetBodyWithCode(response->body(), ResponseErrorCode::ErrorProfileStart);
  }
}

void ProfCpuHandler::DoView(const HttpRequest& request, HttpResponse* response,
                            HttpServerContext* context) {
  std::scoped_lock _(profile_lock_);
  if (!running) {
    response->set_status(HttpStatus::BadRequest);
    SetBodyWithCode(response->body(), ResponseErrorCode::ErrorNotStart);
    return;
  }
  FLARE_LOG_INFO("Profiler flush");
  ProfilerFlush();
  FLARE_LOG_INFO("Pprof start");
  std::string prof_svg;
  int exit_code = 0;
  bool ret = PopenNoShellCompat(
      "pprof --svg " + proc_path_ + " " + kTmpProfileFileName, &prof_svg,
      &exit_code);
  if (!ret || exit_code != 0) {
    FLARE_LOG_ERROR("Failed pprof with code {} ret {}", exit_code, ret);
    response->set_status(HttpStatus::InternalServerError);
    SetBodyWithCode(response->body(), ResponseErrorCode::ErrorProf);
    return;
  }
  response->set_status(HttpStatus::OK);
  response->headers()->Append("Content-Type", "image/svg+xml");
  response->body()->append(prof_svg);
}

void ProfCpuHandler::DoStop(const HttpRequest& request, HttpResponse* response,
                            HttpServerContext* context) {
  std::scoped_lock _(profile_lock_);
  if (!running) {
    response->set_status(HttpStatus::BadRequest);
    SetBodyWithCode(response->body(), ResponseErrorCode::ErrorNotStart);
    return;
  }
  ProfilerStop();
  unlink(kTmpProfileFileName);
  response->set_status(HttpStatus::OK);
  SetBodyWithCode(response->body(), ResponseErrorCode::Succ);
  running = false;
}

void ProfCpuHandler::SetBodyWithCode(std::string* body,
                                     ResponseErrorCode code) {
  static const std::map<ResponseErrorCode, std::string> kProfilerStateMap = {
      {ResponseErrorCode::Succ, "Succ"},
      {ResponseErrorCode::ErrorPath, "Error path, available : start/view/stop"},
      {ResponseErrorCode::ErrorAlreadyStart, "Cpu profile is already started"},
      {ResponseErrorCode::ErrorProfileStart, "ProfileStart failed"},
      {ResponseErrorCode::ErrorNotStart, "Cpu profile is not started"},
      {ResponseErrorCode::ErrorProf, "Run pprof failed"}};
  auto it = kProfilerStateMap.find(code);
  FLARE_CHECK(it != kProfilerStateMap.end());
  Json::Value json_body;
  json_body["code"] = static_cast<int>(code);
  json_body["message"] = std::move(it->second);
  *body = json_body.toStyledString();
}

}  // namespace flare::rpc::builtin
