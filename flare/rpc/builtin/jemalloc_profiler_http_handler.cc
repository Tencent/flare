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

#include "flare/rpc/builtin/jemalloc_profiler_http_handler.h"

#include <dlfcn.h>
#include <fcntl.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "glog/logging.h"
#include "jemalloc/jemalloc.h"
#include "jsoncpp/json.h"

#include "flare/base/string.h"
#include "flare/rpc/builtin/detail/prof_utility.h"

FLARE_RPC_SERVER_REGISTER_BUILTIN_HTTP_PREFIX_HANDLER(
    [](auto&& owner) {
      return std::make_unique<flare::rpc::builtin::JemallocProfilerHttpHandler>(
          "/prof/mem");
    },
    "/prof/mem");

using namespace std::literals;

namespace flare::rpc::builtin {

static constexpr auto kJemallocProfileFile = "heap.prof";

JemallocProfilerHttpHandler::JemallocProfilerHttpHandler(std::string uri_prefix)
    : uri_prefix_(std::move(uri_prefix)) {
#define REGISTER_PPROF_PATH_HANDLER(path, handler)                  \
  prof_path_handler_[path] = [this](auto&& r, auto&& w, auto&& c) { \
    handler(r, w, c);                                               \
  };
  REGISTER_PPROF_PATH_HANDLER("/start", DoStart);
  REGISTER_PPROF_PATH_HANDLER("/view", DoView);
  REGISTER_PPROF_PATH_HANDLER("/stop", DoStop);

  proc_path_ = ReadProcPath();
  size_t sz = sizeof(enabled_);
  // Prof is a read-only value, users should enable it when program starts by
  // setting env MALLOC_CONF.
  if (mallctl("opt.prof", reinterpret_cast<void*>(&enabled_), &sz, nullptr,
              0)) {
    enabled_ = false;
  }
}

void JemallocProfilerHttpHandler::OnGet(const HttpRequest& request,
                                        HttpResponse* response,
                                        HttpServerContext* context) {
  if (!enabled_) {
    response->set_status(HttpStatus::BadRequest);
    SetBodyWithCode(response->body(), ResponseErrorCode::ErrorDisabled);
    return;
  }
  std::string abs_path;
  if (StartsWith(request.uri(), uri_prefix_)) {
    abs_path = request.uri().substr(uri_prefix_.size());
    if (abs_path.back() == '/') {
      abs_path.pop_back();
    }
  }

  if (prof_path_handler_.find(abs_path) != prof_path_handler_.end()) {
    prof_path_handler_[abs_path](request, response, context);
    return;
  }
  response->set_status(HttpStatus::BadRequest);
  SetBodyWithCode(response->body(), ResponseErrorCode::ErrorPath);
}

void JemallocProfilerHttpHandler::DoStart(const HttpRequest& request,
                                          HttpResponse* response,
                                          HttpServerContext* context) {
  std::scoped_lock _(profile_lock_);
  if (running_) {
    response->set_status(HttpStatus::BadRequest);
    SetBodyWithCode(response->body(), ResponseErrorCode::ErrorAlreadyStart);
    return;
  }
  bool start = true;
  if (mallctl("prof.active", nullptr, nullptr, reinterpret_cast<void*>(&start),
              sizeof(start))) {
    response->set_status(HttpStatus::InternalServerError);
    SetBodyWithCode(response->body(), ResponseErrorCode::ErrorMallctl);
    return;
  } else {
    running_ = true;
    response->set_status(HttpStatus::OK);
    SetBodyWithCode(response->body(), ResponseErrorCode::Succ);
  }
}

void JemallocProfilerHttpHandler::DoView(const HttpRequest& request,
                                         HttpResponse* response,
                                         HttpServerContext* context) {
  std::scoped_lock _(profile_lock_);
  if (!running_) {
    response->set_status(HttpStatus::BadRequest);
    SetBodyWithCode(response->body(), ResponseErrorCode::ErrorNotStart);
    return;
  }
  if (mallctl(
          "prof.dump", nullptr, nullptr,
          const_cast<void*>(static_cast<const void*>(&kJemallocProfileFile)),
          sizeof(const char*))) {
    response->set_status(HttpStatus::InternalServerError);
    SetBodyWithCode(response->body(), ResponseErrorCode::ErrorMallctl);
    return;
  }
  ScopedDeferred unlink_file([] { unlink(kJemallocProfileFile); });

  std::string prof_svg;
  int exit_code = 0;
  bool ret = PopenNoShellCompat(
      "jeprof --svg " + proc_path_ + " " + kJemallocProfileFile, &prof_svg,
      &exit_code);
  if (!ret || exit_code != 0) {
    FLARE_LOG_ERROR("Failed jeprof with code {} ret {}", exit_code, ret);
    response->set_status(HttpStatus::InternalServerError);
    SetBodyWithCode(response->body(), ResponseErrorCode::ErrorProf);
    return;
  }
  response->set_status(HttpStatus::OK);
  response->headers()->Append("Content-Type", "image/svg+xml");
  response->body()->append(prof_svg);
}

void JemallocProfilerHttpHandler::DoStop(const HttpRequest& request,
                                         HttpResponse* response,
                                         HttpServerContext* context) {
  std::scoped_lock _(profile_lock_);
  if (!running_) {
    response->set_status(HttpStatus::BadRequest);
    SetBodyWithCode(response->body(), ResponseErrorCode::ErrorNotStart);
    return;
  }
  bool stop = false;
  if (mallctl("prof.active", nullptr, nullptr, reinterpret_cast<void*>(&stop),
              sizeof(stop))) {
    response->set_status(HttpStatus::InternalServerError);
    SetBodyWithCode(response->body(), ResponseErrorCode::ErrorMallctl);
    return;
  }
  response->set_status(HttpStatus::OK);
  SetBodyWithCode(response->body(), ResponseErrorCode::Succ);
  running_ = false;
}

void JemallocProfilerHttpHandler::SetBodyWithCode(std::string* body,
                                                  ResponseErrorCode code) {
  static const std::map<ResponseErrorCode, std::string> kProfilerStateMap = {
      {ResponseErrorCode::Succ, "Succ"},
      {ResponseErrorCode::ErrorPath, "Error path, available : view"},
      {ResponseErrorCode::ErrorDisabled,
       "Prof jemalloc is disabled, you should enable it in MALLOC_CONF"},
      {ResponseErrorCode::ErrorAlreadyStart, "Already started"},
      {ResponseErrorCode::ErrorMallctl, "Failed to call mallctl"},
      {ResponseErrorCode::ErrorWriteFile, "Write file failed"},
      {ResponseErrorCode::ErrorNotStart, "Not started"},
      {ResponseErrorCode::ErrorProf, "Run pprof failed"}};
  auto it = kProfilerStateMap.find(code);
  FLARE_CHECK(it != kProfilerStateMap.end());
  Json::Value json_body;
  json_body["code"] = static_cast<int>(code);
  json_body["message"] = std::move(it->second);
  *body = json_body.toStyledString();
}

}  // namespace flare::rpc::builtin
