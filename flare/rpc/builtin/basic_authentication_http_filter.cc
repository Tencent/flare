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

#include "flare/rpc/builtin/basic_authentication_http_filter.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "flare/base/encoding/base64.h"

using namespace std::literals;

namespace flare {

std::optional<std::pair<std::string, std::string>> ParseCredential(
    const std::string_view& cred) {
  // Authorization: Basic QWxhZGRpbjpPcGVuU2VzYW1l
  static constexpr auto kPrefix = "Basic "sv;
  if (!StartsWith(cred, kPrefix)) {
    return std::nullopt;
  }

  auto decoded = DecodeBase64(cred.substr(kPrefix.size()));
  if (!decoded) {
    return std::nullopt;
  }

  if (auto pos = decoded->find_first_of(':'); pos == std::string::npos) {
    return std::nullopt;
  } else {
    return std::pair(decoded->substr(0, pos), decoded->substr(pos + 1));
  }
}

BasicAuthenticationHttpFilter::BasicAuthenticationHttpFilter(
    VerifyCredential verify_cred, detail::UriMatcher uri_matcher)
    : uri_matcher_(std::move(uri_matcher)),
      cred_verifier_(std::move(verify_cred)) {}

HttpFilter::Action BasicAuthenticationHttpFilter::OnFilter(
    HttpRequest* request, HttpResponse* response, HttpServerContext* context) {
  if (!uri_matcher_(request->uri())) {
    return Action::KeepProcessing;
  }

  auto cred_opt =
      ParseCredential(request->headers()->TryGet("Authorization").value_or(""));
  if (!cred_opt /* Invalid credential. */ ||
      !cred_verifier_(cred_opt->first,
                      cred_opt->second) /* Credential not accepted. */) {
    GenerateDefaultResponsePage(HttpStatus::Unauthorized, response);
    response->headers()->Append("WWW-Authenticate",
                                R"(Basic realm="Authorization required.")");
    return Action::EarlyReturn;
  }

  return Action::KeepProcessing;
}

}  // namespace flare
