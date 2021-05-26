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

#include "flare/base/encoding/percent.h"

#include <array>
#include <string>
#include <string_view>

#include "flare/base/encoding/detail/hex_chars.h"
#include "flare/base/enum.h"
#include "flare/base/logging.h"

using namespace std::literals;

namespace flare {

namespace {

// Alphabets / numeric characters need not to be listed in `unescaped_chars`.
constexpr std::array<bool, 256> GenerateUnescapedCharBitmap(
    const std::string_view& unescaped_chars) {
  std::array<bool, 256> result{};
  for (auto&& e : unescaped_chars) {
    result[e] = true;
  }
  for (int i = 0; i != 10; ++i) {
    result[i + '0'] = true;
  }
  for (int i = 0; i != 26; ++i) {
    result[i + 'a'] = true;
    result[i + 'A'] = true;
  }
  return result;
}

constexpr auto kUnescapedChars = [] {
  auto emca262 =
      std::array{GenerateUnescapedCharBitmap("_-,;:!?.'()@*/&#+=~$"sv),
                 GenerateUnescapedCharBitmap("_-!.*~'()"sv)};
  auto rfc3986 =
      std::array{GenerateUnescapedCharBitmap("_-,;:!?.'()[]@*/&#+=~$"sv),
                 GenerateUnescapedCharBitmap("_-.~"sv)};
  auto rfc5987 =
      // No "reserved" characters, both bitmap are the same.
      std::array{GenerateUnescapedCharBitmap("!#$&+-.^_`|~"sv),
                 GenerateUnescapedCharBitmap("!#$&+-.^_`|~"sv)};
  return std::array{emca262, rfc3986, rfc5987};
}();

}  // namespace

std::string EncodePercent(const std::string_view& from,
                          const PercentEncodingOptions& options) {
  std::string result;
  EncodePercent(from, &result, options);
  return result;
}

std::optional<std::string> DecodePercent(const std::string_view& from,
                                         bool decode_plus_sign_as_whitespace) {
  std::string result;
  if (DecodePercent(from, &result, decode_plus_sign_as_whitespace)) {
    return result;
  }
  return std::nullopt;
}

void EncodePercent(const std::string_view& from, std::string* to,
                   const PercentEncodingOptions& options) {
  auto&& unescaped =
      kUnescapedChars[underlying_value(options.style)][options.escape_reserved];
  for (auto&& e : from) {
    if (unescaped[static_cast<std::uint8_t>(e)]) {
      to->push_back(e);
    } else {
      // @sa: RFC3986:
      //
      // > For consistency, URI producers and normalizers should use uppercase
      // > hexadecimal digits for all percent encodings.
      auto hex_chars = detail::kHexCharsUppercase[static_cast<std::uint8_t>(e)];
      to->append({'%', hex_chars.a, hex_chars.b});
    }
  }
}

bool DecodePercent(const std::string_view& from, std::string* to,
                   bool decode_plus_sign_as_whitespace) {
  // We may over-allocate here, that won't hurt.
  to->reserve(from.size());
  for (auto iter = from.begin(); iter != from.end();) {
    if (*iter == '%') {
      if (iter + 3 > from.end()) {
        return false;
      }
      auto v = detail::AsciiCodeFromHexCharPair(*(iter + 1), *(iter + 2));
      if (v == -1) {
        return false;
      }
      to->push_back(v);
      iter += 3;
    } else {
      if (decode_plus_sign_as_whitespace && *iter == '+') {
        to->push_back(' ');
        ++iter;
      } else {
        to->push_back(*iter++);
      }
    }
  }
  return true;
}

}  // namespace flare
