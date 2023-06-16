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

#include "flare/rpc/builtin/tcmalloc_profiler_http_handler.h"

#include <dlfcn.h>
#include <fcntl.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "glog/logging.h"
#include "gperftools/malloc_extension.h"
#include "jsoncpp/json.h"

#include "flare/base/string.h"
#include "flare/rpc/builtin/detail/prof_utility.h"

FLARE_RPC_SERVER_REGISTER_BUILTIN_HTTP_PREFIX_HANDLER(
    [](auto&& owner) {
      return std::make_unique<flare::rpc::builtin::TcmallocProfilerHttpHandler>(
          "/prof/mem");
    },
    "/prof/mem");

using namespace std::literals;

namespace flare::rpc::builtin {

static constexpr auto kGperftoolsProfileFile = "heap.prof";

namespace {

// The `content' should be small so that it can be written into file in one
// fwrite (at most time).
bool WriteSmallFile(const char* filepath, const std::string& content) {
  FILE* fp = fopen(filepath, "w");
  if (!fp) {
    FLARE_LOG_WARNING("Fail to open {}", filepath);
    return false;
  }

  auto n = fwrite(content.data(), content.size(), 1UL, fp);
  fclose(fp);
  if (n != 1UL) {
    FLARE_LOG_WARNING("Fail to write into {}", filepath);
    return false;
  }
  return true;
}

}  // namespace

TcmallocProfilerHttpHandler::TcmallocProfilerHttpHandler(std::string uri_prefix)
    : uri_prefix_(std::move(uri_prefix)) {
#define REGISTER_PPROF_PATH_HANDLER(path, handler) \
  handlers_[path] = [this](auto&& r, auto&& w, auto&& c) { handler(r, w, c); };
  REGISTER_PPROF_PATH_HANDLER("/view", DoView);

  proc_path_ = ReadProcPath();
}

void TcmallocProfilerHttpHandler::OnGet(const HttpRequest& request,
                                        HttpResponse* response,
                                        HttpServerContext* context) {
  std::string abs_path;
  if (StartsWith(request.uri(), uri_prefix_)) {
    abs_path = request.uri().substr(uri_prefix_.size());
    if (abs_path.back() == '/') {
      abs_path.pop_back();
    }
  }

  if (handlers_.find(abs_path) != handlers_.end()) {
    handlers_[abs_path](request, response, context);
    return;
  }
  response->set_status(HttpStatus::BadRequest);
  SetBodyWithCode(response->body(), ResponseErrorCode::ErrorPath);
}

void TcmallocProfilerHttpHandler::DoView(const HttpRequest& request,
                                         HttpResponse* response,
                                         HttpServerContext* context) {
  if (!getenv("TCMALLOC_SAMPLE_PARAMETER")) {
    response->set_status(HttpStatus::InternalServerError);
    SetBodyWithCode(response->body(),
                    ResponseErrorCode::ErrorNotSetSampleParamter);
    return;
  }

  std::scoped_lock _(profile_lock_);
  MallocExtension* malloc_ext = MallocExtension::instance();
  FLARE_CHECK(malloc_ext,
              "Tcmalloc is linked, malloc ext should always be not null.");
  std::string obj;
  malloc_ext->GetHeapSample(&obj);
  if (!WriteSmallFile(kGperftoolsProfileFile, obj)) {
    response->set_status(HttpStatus::InternalServerError);
    SetBodyWithCode(response->body(), ResponseErrorCode::ErrorWriteFile);
    return;
  }
  std::string prof_svg;
  int exit_code = 0;
  bool ret = PopenNoShellCompat(
      "pprof --svg " + proc_path_ + " " + kGperftoolsProfileFile, &prof_svg,
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
  unlink(kGperftoolsProfileFile);
}

void TcmallocProfilerHttpHandler::SetBodyWithCode(std::string* body,
                                                  ResponseErrorCode code) {
  static const std::map<ResponseErrorCode, std::string> kProfilerStateMap = {
      {ResponseErrorCode::Succ, "Succ"},
      {ResponseErrorCode::ErrorNotSetSampleParamter,
       "SampleParamter is not set, you should set env "
       "TCMALLOC_SAMPLE_PARAMETER, recommend value : 524288"},
      {ResponseErrorCode::ErrorPath, "Error path, available : view"},
      {ResponseErrorCode::ErrorWriteFile, "Write file failed"},
      {ResponseErrorCode::ErrorProf, "Run pprof failed"}};
  auto it = kProfilerStateMap.find(code);
  FLARE_CHECK(it != kProfilerStateMap.end());
  Json::Value json_body;
  json_body["code"] = static_cast<int>(code);
  json_body["message"] = std::move(it->second);
  *body = json_body.toStyledString();
}

}  // namespace flare::rpc::builtin
