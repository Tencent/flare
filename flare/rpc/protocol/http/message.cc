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

#include "flare/rpc/protocol/http/message.h"

#include <utility>

namespace flare::http {

/*---------------------------HttpRequestMessage-------------------------*/

HttpRequestMessage::HttpRequestMessage(HttpRequest http_request)
    : http_request_(std::move(http_request)) {
  SetRuntimeTypeTo<HttpRequestMessage>();
}

std::uint64_t HttpRequestMessage::GetCorrelationId() const noexcept {
  return 0;
}

Message::Type HttpRequestMessage::GetType() const noexcept {
  return Type::Single;
}

/*----------------------------HttpResponseMessage-------------------------*/

HttpResponseMessage::HttpResponseMessage(HttpResponse http_response)
    : http_response_(std::move(http_response)) {
  SetRuntimeTypeTo<HttpResponseMessage>();
}

std::uint64_t HttpResponseMessage::GetCorrelationId() const noexcept {
  return 0;  // 0 is regard as a invalid rpc sequence number.
}

Message::Type HttpResponseMessage::GetType() const noexcept {
  return Type::Single;  // FIXME: For first message in `chunked` encoding,
                        // this should be StreamStart. How to test this?
}

}  // namespace  flare::http
