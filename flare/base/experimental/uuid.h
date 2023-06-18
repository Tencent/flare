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

#ifndef FLARE_BASE_EXPERIMENTAL_UUID_H_
#define FLARE_BASE_EXPERIMENTAL_UUID_H_

#include <cstddef>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>

#include "flare/base/logging.h"

namespace flare::experimental {

// Represents a UUID.
class Uuid {
 public:
  constexpr Uuid() = default;

  // If `from` is malformed, the program crashes.
  //
  // To parse UUID from untrusted source, use `TryParse<Uuid>(...)` instead.
  constexpr explicit Uuid(std::string_view from);

  std::string ToString() const;

  // TODO(luobogao): Leverage default comparions to simplify these overloads
  // once it's available.
  constexpr bool operator==(const Uuid& other) const noexcept;
  constexpr bool operator!=(const Uuid& other) const noexcept;
  constexpr bool operator<(const Uuid& other) const noexcept;

 private:
  constexpr int CompareTo(const Uuid& other) const noexcept;
  constexpr static int ToDecimal(char x);
  constexpr static std::uint8_t ToUInt8(const char* starts_at);

 private:
  std::uint8_t bytes_[16] = {0};
};

////////////////////////////////////////
// Implementation goes below.         //
////////////////////////////////////////

constexpr Uuid::Uuid(std::string_view from) {
  FLARE_CHECK_EQ(from.size(), 36);  // 8-4-4-4-12
  auto p = from.data();

  bytes_[0] = ToUInt8(p);
  bytes_[1] = ToUInt8(p + 2);
  bytes_[2] = ToUInt8(p + 4);
  bytes_[3] = ToUInt8(p + 6);
  p += 8;
  FLARE_CHECK_EQ(*p++, '-');

  bytes_[4] = ToUInt8(p);
  bytes_[5] = ToUInt8(p + 2);
  p += 4;
  FLARE_CHECK_EQ(*p++, '-');

  bytes_[6] = ToUInt8(p);
  bytes_[7] = ToUInt8(p + 2);
  p += 4;
  FLARE_CHECK_EQ(*p++, '-');

  bytes_[8] = ToUInt8(p);
  bytes_[9] = ToUInt8(p + 2);
  p += 4;
  FLARE_CHECK_EQ(*p++, '-');

  bytes_[10] = ToUInt8(p);
  bytes_[11] = ToUInt8(p + 2);
  bytes_[12] = ToUInt8(p + 4);
  bytes_[13] = ToUInt8(p + 6);
  bytes_[14] = ToUInt8(p + 8);
  bytes_[15] = ToUInt8(p + 10);
}

constexpr bool Uuid::operator==(const Uuid& other) const noexcept {
  return CompareTo(other) == 0;
}

constexpr bool Uuid::operator!=(const Uuid& other) const noexcept {
  return CompareTo(other) != 0;
}

constexpr bool Uuid::operator<(const Uuid& other) const noexcept {
  return CompareTo(other) < 0;
}

constexpr int Uuid::CompareTo(const Uuid& other) const noexcept {
  // `memcmp` is not `constexpr`..
  return __builtin_memcmp(bytes_, other.bytes_, sizeof(bytes_));
}

constexpr int Uuid::ToDecimal(char x) {
  if (x >= '0' && x <= '9') {
    return x - '0';
  } else if (x >= 'a' && x <= 'f') {
    return x - 'a' + 10;
  } else if (x >= 'A' && x <= 'F') {
    return x - 'A' + 10;
  } else {
    FLARE_CHECK(0, "Invalid hex digit [{}].", x);
  }
}

constexpr std::uint8_t Uuid::ToUInt8(const char* starts_at) {  // `ToUint8`?
  return ToDecimal(starts_at[0]) * 16 + ToDecimal(starts_at[1]);
}

}  // namespace flare::experimental

namespace flare {

template <class T, class>
struct TryParseTraits;

template <>
struct TryParseTraits<experimental::Uuid, void> {
  static std::optional<experimental::Uuid> TryParse(std::string_view s);
};

}  // namespace flare

#endif  // FLARE_BASE_EXPERIMENTAL_UUID_H_
