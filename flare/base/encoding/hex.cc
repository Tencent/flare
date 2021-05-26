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

#include "flare/base/encoding/hex.h"

#include <string>
#include <string_view>

#include "flare/base/encoding/detail/hex_chars.h"
#include "flare/base/logging.h"

namespace flare {

std::string EncodeHex(const std::string_view& from, bool uppercase) {
  std::string result;
  EncodeHex(from, &result, uppercase);
  return result;
}

std::optional<std::string> DecodeHex(const std::string_view& from) {
  std::string result;
  DecodeHex(from, &result);
  return result;
}

void EncodeHex(const std::string_view& from, std::string* to, bool uppercase) {
  to->reserve(from.size() * 2);
  for (auto&& e : from) {
    auto index = static_cast<std::uint8_t>(e);
    if (uppercase) {
      to->append({detail::kHexCharsUppercase[index].a,
                  detail::kHexCharsUppercase[index].b});
    } else {
      to->append({detail::kHexCharsLowercase[index].a,
                  detail::kHexCharsLowercase[index].b});
    }
  }
}

bool DecodeHex(const std::string_view& from, std::string* to) {
  if (from.size() % 2 != 0) {
    return false;
  }
  to->reserve(from.size() / 2);
  for (int i = 0; i != from.size(); i += 2) {
    auto v = detail::AsciiCodeFromHexCharPair(from[i], from[i + 1]);
    if (v == -1) {
      return false;
    }
    FLARE_CHECK(v >= 0 && v <= 255);
    to->push_back(v);
  }
  return true;
}

}  // namespace flare
