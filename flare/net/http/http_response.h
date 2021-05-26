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

#ifndef FLARE_NET_HTTP_HTTP_RESPONSE_H_
#define FLARE_NET_HTTP_HTTP_RESPONSE_H_

#include <optional>
#include <string>
#include <string_view>

#include "flare/base/buffer.h"
#include "flare/net/http/http_message.h"
#include "flare/net/http/types.h"

namespace flare {

// HTTP response message.
class HttpResponse : private http::HttpMessage {
 public:
  // You shouldn't be setting `version` in most cases. The framework will do it
  // for you.
  using HttpMessage::set_version;
  using HttpMessage::version;

  HttpStatus status() const noexcept { return status_; }
  void set_status(HttpStatus status) noexcept { status_ = status; }

  using HttpMessage::body;
  using HttpMessage::body_size;
  using HttpMessage::headers;
  using HttpMessage::noncontiguous_body;
  using HttpMessage::set_body;

  void clear() noexcept;

  // PERFORMS BADLY. It's provided for debugging purpose only.
  std::string ToString() const;

 private:
  HttpStatus status_{HttpStatus::OK};
};

void GenerateDefaultResponsePage(HttpStatus status, HttpResponse* response,
                                 const std::string_view& title = "",
                                 const std::string_view& body = "");

namespace http {

// FOR INTERNAL USE.
const std::string_view& GetStatusCodeWithDescString(HttpStatus status) noexcept;

}  // namespace http

}  // namespace flare

#endif  // FLARE_NET_HTTP_HTTP_RESPONSE_H_
