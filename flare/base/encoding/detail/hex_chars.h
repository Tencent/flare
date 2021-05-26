
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

#ifndef FLARE_BASE_ENCODING_DETAIL_HEX_CHARS_H_
#define FLARE_BASE_ENCODING_DETAIL_HEX_CHARS_H_

#include <array>

#include "flare/base/logging.h"

namespace flare::detail {

struct CharPair {
  char a, b;
};

constexpr auto kHexCharsLowercase = [] {
  char chars[] = "0123456789abcdef";
  std::array<CharPair, 256> result{};
  for (int i = 0; i != 256; ++i) {
    result[i].a = chars[(i >> 4) & 0xF];
    result[i].b = chars[i & 0xF];
  }
  return result;
}();

constexpr auto kHexCharsUppercase = [] {
  char chars[] = "0123456789ABCDEF";
  std::array<CharPair, 256> result{};
  for (int i = 0; i != 256; ++i) {
    result[i].a = chars[(i >> 4) & 0xF];
    result[i].b = chars[i & 0xF];
  }
  return result;
}();

constexpr auto kHexCharToDecimal = [] {
  std::array<int, 256> result{};
  for (auto&& e : result) {
    e = -1;
  }
  for (int i = 0; i != 10; ++i) {
    result[i + '0'] = i;
  }
  for (int i = 0; i != 6; ++i) {
    result[i + 'a'] = 10 + i;
    result[i + 'A'] = 10 + i;
  }
  return result;
}();

inline int AsciiCodeFromHexCharPair(std::uint8_t x, std::uint8_t y) {
  auto a = kHexCharToDecimal[x], b = kHexCharToDecimal[y];
  if (a == -1 || b == -1) {
    return -1;
  }
  FLARE_DCHECK(a >= 0 && a < 16 && b >= 0 && b < 16);
  return a * 16 + b;
}

}  // namespace flare::detail

#endif  // FLARE_BASE_ENCODING_DETAIL_HEX_CHARS_H_
