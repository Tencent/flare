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

#include "flare/net/http/http_request.h"

#include <string_view>
#include <unordered_map>

#include "flare/base/enum.h"
#include "flare/base/internal/early_init.h"
#include "flare/net/http/http_headers.h"

using namespace std::literals;

namespace flare {

void HttpRequest::clear() noexcept {
  HttpMessage::clear();
  method_ = HttpMethod::Unspecified;
  uri_.clear();
}

std::string HttpRequest::ToString() const {
  std::string result;

  result += Format("{} {} {}\r\n", ToStringView(method_), uri_,
                   ToStringView(version()));
  for (auto&& [k, v] : *headers()) {
    result += Format("{}: {}\r\n", k, v);
  }
  result += "\r\n";
  if (auto ptr = noncontiguous_body()) {
    result += FlattenSlow(*ptr);
  } else {
    result += *body();
  }
  return result;
}

}  // namespace flare
