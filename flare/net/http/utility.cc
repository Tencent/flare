// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "flare/net/http/utility.h"

#include <string>

#include "flare/base/net/endpoint.h"
#include "flare/base/string.h"

using namespace std::literals;

namespace flare {

namespace {

// Determines if `ip` is a valid address, and construct an `Endpoint` for it.
std::optional<Endpoint> TryParseIpAsEndpoint(const std::string_view& ip) {
  std::string fake_ep;
  if (ip.find(':') == std::string_view::npos) {
    fake_ep = Format("{}:0", ip);
  } else {
    fake_ep = Format("[{}]:0", ip);
  }
  return TryParse<Endpoint>(fake_ep);
}

}  // namespace

std::optional<std::string> TryGetOriginatingIp(const HttpRequest& request) {
  auto ep = TryGetOriginatingEndpoint(request);
  if (ep) {
    return EndpointGetIp(*ep);
  }
  return std::nullopt;
}

std::optional<Endpoint> TryGetOriginatingEndpoint(const HttpRequest& request) {
  auto&& headers = *request.headers();

  if (auto opt = headers.TryGet("X-Forwarded-For")) {
    auto addrs = Split(*opt, ",");
    if (!addrs.empty()) {
      // The first one is client's IP then.
      if (auto ep = TryParseIpAsEndpoint(addrs[0])) {
        return ep;
      }  // Fall-through then.
    }
  }
  if (auto opt = headers.TryGet("X-Real-IP")) {
    if (auto ep = TryParseIpAsEndpoint(*opt)) {
      return ep;
    }  // Fall-through then.
  }
  return std::nullopt;
}

}  // namespace flare
