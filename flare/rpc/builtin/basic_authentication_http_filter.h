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

#ifndef FLARE_RPC_BUILTIN_BASIC_AUTHENTICATION_HTTP_FILTER_H_
#define FLARE_RPC_BUILTIN_BASIC_AUTHENTICATION_HTTP_FILTER_H_

#include <string_view>

#include "flare/base/function.h"
#include "flare/rpc/builtin/detail/uri_matcher.h"
#include "flare/rpc/http_filter.h"

namespace flare {

// Implements "basic" authentication scheme.
class BasicAuthenticationHttpFilter : public HttpFilter {
  // Called with `user` / `password`.
  using VerifyCredential =
      Function<bool(std::string_view user, std::string_view password)>;

 public:
  // `verify_cred` is called to determine if the credential provided by the user
  // is acceptable.
  explicit BasicAuthenticationHttpFilter(VerifyCredential verify_cred,
                                         detail::UriMatcher uri_matcher = {});

  Action OnFilter(HttpRequest* request, HttpResponse* response,
                  HttpServerContext* context) override;

 private:
  detail::UriMatcher uri_matcher_;
  VerifyCredential cred_verifier_;
};

// TODO(luobogao): `LoadHtpasswdFileAsCredentialVerifier`.

}  // namespace flare

#endif  // FLARE_RPC_BUILTIN_BASIC_AUTHENTICATION_HTTP_FILTER_H_
