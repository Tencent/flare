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

#ifndef FLARE_NET_HTTP_HTTP_REQUEST_H_
#define FLARE_NET_HTTP_HTTP_REQUEST_H_

#include <optional>
#include <string>
#include <utility>

#include "flare/base/string.h"
#include "flare/net/http/http_message.h"
#include "flare/net/http/types.h"

namespace flare {

// HTTP request message.
class HttpRequest : private http::HttpMessage {
 public:
  // Note that version is ignored by `HttpClient`, we select the best version to
  // use automatically. It's provided here for server-side.
  using HttpMessage::set_version;
  using HttpMessage::version;

  HttpMethod method() const noexcept { return method_; }
  void set_method(HttpMethod method) noexcept { method_ = method; }

  const std::string& uri() const noexcept { return uri_; }
  void set_uri(std::string s) noexcept { uri_ = std::move(s); }

  using HttpMessage::body;
  using HttpMessage::body_size;
  using HttpMessage::headers;
  using HttpMessage::noncontiguous_body;
  using HttpMessage::set_body;

  void clear() noexcept;

  // PERFORMS BADLY. It's provided for debugging purpose only.
  std::string ToString() const;

 private:
  HttpMethod method_{HttpMethod::Unspecified};
  std::string uri_;
};

}  // namespace flare

#endif  // FLARE_NET_HTTP_HTTP_REQUEST_H_
