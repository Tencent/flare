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

#include "flare/rpc/protocol/http/builtin/misc_handler.h"

#include <sstream>
#include <string>
#include <unordered_map>

#include "thirdparty/jsoncpp/json.h"

#include "flare/base/string.h"

using namespace std::literals;

FLARE_RPC_SERVER_REGISTER_BUILTIN_HTTP_HANDLER(flare::rpc::builtin::MiscHandler,
                                               "/inspect/status",
                                               "/inspect/version");

extern "C" {
namespace binary_version {  // @sa: `common/base/binary_version.cc`.

// Not declare as const intentionally, otherwise the compiler will inline access
// to it.
[[gnu::weak]] int kSvnInfoCount = 0;
[[gnu::weak]] extern const char* const kSvnInfo[];
[[gnu::weak]] extern const char kBuildType[] = "Unknown";
[[gnu::weak]] extern const char kBuildTime[] = "Unknown";
[[gnu::weak]] extern const char kBuilderName[] = "Unknown";
[[gnu::weak]] extern const char kHostName[] = "Unknown";
[[gnu::weak]] extern const char kCompiler[] = "Unknown";

}  // namespace binary_version
}

namespace flare::rpc::builtin {

static const auto kProcessStartTime = time(nullptr);

namespace {

std::string GetProcessorStartupTimeString() {
  struct tm tm;
  char time[128];

  localtime_r(&kProcessStartTime, &tm);
  auto len = strftime(time, sizeof(time), "%Y%m%d%H%M%S", &tm);
  return std::string(time, len);
}

// Copied from `common/base/binary_version.cc`.
std::string GetVersionInfo() {
  using namespace binary_version;

  std::ostringstream oss;
  oss << "\n";  // Open a new line in gflags --version output.

  oss << "BuildTime: " << kBuildTime << "\n"
      << "BuildType: " << kBuildType << "\n"
      << "BuilderName: " << kBuilderName << "\n"
      << "HostName: " << kHostName << "\n"
      << "Compiler: " << kCompiler << "\n";

  if (kSvnInfoCount > 0) {
    std::string line_breaker(100, '-');  // ----------
    oss << "Sources:\n" << line_breaker << "\n";
    for (int i = 0; i < kSvnInfoCount; ++i) oss << kSvnInfo[i];
    oss << line_breaker << "\n";
  }

  return oss.str();
}

}  // namespace

void MiscHandler::OnGet(const HttpRequest& request, HttpResponse* response,
                        HttpServerContext* context) {
  using H = decltype(&MiscHandler::OnGetStatus);

  constexpr auto kInspectPrefix = "/inspect/"sv;
  static const std::unordered_map<std::string_view, H> kHandler = {
      {"status"sv, &MiscHandler::OnGetStatus},
      {"version"sv, &MiscHandler::OnGetVersion}};

  FLARE_CHECK(StartsWith(request.uri(), "/inspect/"), "Unexpected URI [{}].",
              request.uri());
  auto rest_path = request.uri().substr(kInspectPrefix.size());
  auto iter = kHandler.find(rest_path);

  FLARE_CHECK(iter != kHandler.end(), "Unexpected URI [{]].", request.uri());
  return (this->*(iter->second))(request, response, context);
}

void MiscHandler::OnGetVersion(const HttpRequest& request,
                               HttpResponse* response,
                               HttpServerContext* context) {
  response->set_status(HttpStatus::OK);
  response->set_body(GetVersionInfo());
}

void MiscHandler::OnGetStatus(const HttpRequest& request,
                              HttpResponse* response,
                              HttpServerContext* context) {
  Json::Value jsr;
  jsr["status"] = "SERVER_STATUS_OK";  // Other statuses are transient anyway.
  jsr["process"]["start_time"] = GetProcessorStartupTimeString();

  // Other information about `owner_` goes here.

  response->set_status(HttpStatus::OK);
  response->headers()->Append("Content-Type", "application/json");
  response->set_body(Json::StyledWriter().write(jsr));
}

}  // namespace flare::rpc::builtin
