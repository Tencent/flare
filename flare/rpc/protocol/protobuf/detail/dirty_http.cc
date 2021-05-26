
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

#include "flare/rpc/protocol/protobuf/detail/dirty_http.h"

#include <string>
#include <string_view>

#include "flare/base/string.h"

namespace flare::protobuf {

std::string_view TryGetHeaderRoughly(const std::string& header,
                                     const std::string& key) {
  auto ptr = strcasestr(header.data(), key.data());
  if (!ptr || ptr == header.data() || ptr[-1] != '\n') {
    return "";
  }
  auto eol = strchr(ptr, '\r');  // Given that `header` is terminated with
                                 // `\r\n`, `eol` can't be nullptr.
  auto sep = strchr(ptr, ':');
  if (!sep) {
    return "";
  }
  return Trim(std::string_view(sep + 1, eol - (sep + 1)));
}

}  // namespace flare::protobuf
