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

#ifndef FLARE_NET_HTTP_UTILITY_H_
#define FLARE_NET_HTTP_UTILITY_H_

#include <optional>
#include <string>

#include "flare/base/net/endpoint.h"
#include "flare/net/http/http_request.h"

namespace flare {

// Get originating IP of the request. If the originating IP is not supplied in
// the header (e.g., there's no proxy in between), you can use `remote_peer`
// in `HttpServerContext`.
std::optional<std::string> TryGetOriginatingIp(const HttpRequest& request);

// Same as `TryGetOriginatingIp`, but it also provide address family. Note that
// originating port is not available, it's always 0.
//
// This method simplifies things a bit if you want to write
// `TryGetOriginatingEndpoint(req).value_or(ctx->remote_peer)`.
std::optional<Endpoint> TryGetOriginatingEndpoint(const HttpRequest& request);

}  // namespace flare

#endif  // FLARE_NET_HTTP_UTILITY_H_
