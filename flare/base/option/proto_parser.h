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

#ifndef FLARE_BASE_OPTION_PROTO_PARSER_H_
#define FLARE_BASE_OPTION_PROTO_PARSER_H_

#include <optional>
#include <string>

#include "google/protobuf/text_format.h"

namespace flare::option {

// Parses "debug string" to Protocol Buffers message.
template <class T>
struct ProtoTextFormatParser {
  static std::optional<T> TryParse(const std::string& str) {
    T result;
    if (google::protobuf::TextFormat::ParseFromString(str, &result)) {
      return result;
    }
    return std::nullopt;
  }
};

}  // namespace flare::option

#endif  // FLARE_BASE_OPTION_PROTO_PARSER_H_
