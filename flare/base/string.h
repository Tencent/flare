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

#ifndef FLARE_BASE_STRING_H_
#define FLARE_BASE_STRING_H_

#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "fmt/format.h"
#include "fmt/ostream.h"

namespace flare {

template <class T, class = void>
struct TryParseTraits;

// Try parse `s` as `T`.
//
// To "specialize" `flare::TryParse` for your own type, either implement
// `TryParse` in a namespace that can be found via ADL, with signature like ...
//
//   std::optional<T> TryParse(
//       std::common_type<T>, const std::string_view& s, ...);
//
// ... or specialize `TryParseTraits<T>`.
//
// FIXME: The first approach does not work yet.
template <class T, class... Args>
inline std::optional<T> TryParse(const std::string_view& s,
                                 const Args&... args) {
  return TryParseTraits<T>::TryParse(s, args...);
}

// @sa: `std::format`
template <class... Args>
std::string Format(const std::string_view& fmt, Args&&... args) {
  return fmt::format(fmt, std::forward<Args>(args)...);
}

// `std::string(_view)::starts_with/ends_with` is not available until C++20, so
// we roll our own here.
bool StartsWith(const std::string_view& s, const std::string_view& prefix);
bool EndsWith(const std::string_view& s, const std::string_view& suffix);

// Replace occurrance of `from` in `str` to `to` for at most `count` times.
void Replace(const std::string_view& from, const std::string_view& to,
             std::string* str,
             std::size_t count = std::numeric_limits<std::size_t>::max());
std::string Replace(
    const std::string_view& str, const std::string_view& from,
    const std::string_view& to,
    std::size_t count = std::numeric_limits<std::size_t>::max());

// Trim whitespace at both end of the string.
std::string_view Trim(const std::string_view& str);

// Split string by `delim`.
std::vector<std::string_view> Split(const std::string_view& s, char delim,
                                    bool keep_empty = false);
std::vector<std::string_view> Split(const std::string_view& s,
                                    const std::string_view& delim,
                                    bool keep_empty = false);
// void Split(const std::string_view& s,
//            Function<std::size_t(const std::string_view&)> finder,
//            std::vector<std::string_view>* splited, bool keep_empty = false);

// Join strings in `parts`, delimited by `delim`.
std::string Join(const std::vector<std::string_view>& parts,
                 const std::string_view& delim);
std::string Join(const std::vector<std::string>& parts,
                 const std::string_view& delim);
std::string Join(const std::initializer_list<std::string_view>& parts,
                 const std::string_view& delim);

// To uppercase / lowercase.
//
// TODO(luobogao): locale?
char ToUpper(char c);
char ToLower(char c);
void ToUpper(std::string* s);
void ToLower(std::string* s);
std::string ToUpper(const std::string_view& s);
std::string ToLower(const std::string_view& s);

// Case insensitive-comparison.
//
// Deprecated. Use `ICompare` instead.
bool IEquals(const std::string_view& first, const std::string_view& second);

// ICompare is reserved for now, so we can return `std::strong_ordering` once
// C++20 is available.

// Implementation goes below.

template <class T, class>
struct TryParseTraits {
  // The default implementation delegates calls to `TryParse` found by ADL.
  template <class... Args>
  static std::optional<T> TryParse(const std::string_view& s,
                                   const Args&... args) {
    return TryParse(std::common_type<T>(), s, args...);
  }
};

template <>
struct TryParseTraits<bool> {
  // For numerical values, only 0 and 1 are recognized, all other numeric values
  // lead to parse failure (i.e., `std::nullopt` is returned).
  //
  // If `recognizes_alphabet_symbol` is set, following symbols are recognized:
  //
  // - "true" / "false"
  // - "y" / "n"
  // - "yes" / "no"
  //
  // For all other symbols, we treat them as neither `true` nor `false`.
  // `std::nullopt` is returned in those cases.
  static std::optional<bool> TryParse(const std::string_view& s,
                                      bool recognizes_alphabet_symbol = true,
                                      bool ignore_case = true);
};

template <class T>
struct TryParseTraits<T, std::enable_if_t<std::is_integral_v<T>>> {
  static std::optional<T> TryParse(const std::string_view& s, int base = 10);
};

template <class T>
struct TryParseTraits<T, std::enable_if_t<std::is_floating_point_v<T>>> {
  static std::optional<T> TryParse(const std::string_view& s);
};

}  // namespace flare

#endif  // FLARE_BASE_STRING_H_
