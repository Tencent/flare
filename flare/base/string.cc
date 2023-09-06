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

#include "flare/base/string.h"

#include <array>
#include <charconv>
#include <climits>
#include <cstdlib>
#include <optional>

#include "flare/base/internal/logging.h"

namespace flare {

namespace {

constexpr std::array<char, 256> kLowerChars = []() {
  std::array<char, 256> cs{};
  for (std::size_t index = 0; index != 256; ++index) {
    if (index >= 'A' && index <= 'Z') {
      cs[index] = index ^ 32;
    } else {
      cs[index] = index;
    }
  }
  return cs;
}();

constexpr std::array<char, 256> kUpperChars = []() {
  std::array<char, 256> cs{};
  for (std::size_t index = 0; index != 256; ++index) {
    if (index >= 'a' && index <= 'z') {
      cs[index] = index ^ 32;
    } else {
      cs[index] = index;
    }
  }
  return cs;
}();

template <class T, class F, class... Args>
std::optional<T> TryParseImpl(F fptr, const char* s,
                              std::initializer_list<T> invs, Args... args) {
  if (FLARE_UNLIKELY(s[0] == 0)) {
    return std::nullopt;
  }

  struct LastErrorStasher {
    LastErrorStasher() : last_errno(errno) { errno = 0; }
    ~LastErrorStasher() { errno = last_errno; }
    int last_errno;
  } _;

  char* end;
  auto result = fptr(s, &end, args...);
  if (end != s + strlen(s)) {  // Return value `0` is also handled by this test.
    return std::nullopt;
  }
  for (auto&& e : invs) {
    if (result == e && errno == ERANGE) {
      return std::nullopt;
    }
  }
  return result;
}

template <class T, class U>
std::optional<T> TryNarrowCast(U&& opt) {
  if (!opt || *opt > std::numeric_limits<T>::max() ||
      *opt < std::numeric_limits<T>::min()) {
    return std::nullopt;
  }
  return opt;
}

template <class T>
void JoinImpl(const T& parts, std::string_view delim, std::string* result) {
  auto size = 0;
  for (auto&& e : parts) {
    size += e.size() + delim.size();
  }
  result->clear();
  if (!size) {
    return;
  }
  size -= delim.size();
  result->reserve(size);
  for (auto iter = parts.begin(); iter != parts.end(); ++iter) {
    if (iter != parts.begin()) {
      result->append(delim.begin(), delim.end());
    }
    result->append(iter->begin(), iter->end());
  }
  return;
}

}  // namespace

bool StartsWith(std::string_view s, std::string_view prefix) {
  return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

bool EndsWith(std::string_view s, std::string_view suffix) {
  return s.size() >= suffix.size() &&
         s.substr(s.size() - suffix.size()) == suffix;
}

void Replace(std::string_view from, std::string_view to, std::string* str,
             std::size_t count) {
  FLARE_CHECK(!from.empty(), "`from` may not be empty.");
  auto p = str->find(from);
  while (p != std::string::npos && count--) {
    str->replace(p, from.size(), to);
    p = str->find(from, p + to.size());
  }
}

std::string Replace(std::string_view str, std::string_view from,
                    std::string_view to, std::size_t count) {
  std::string cp(str);
  Replace(from, to, &cp, count);
  return cp;
}

std::string_view Trim(std::string_view str) {
  std::size_t s = 0, e = str.size();
  if (str.empty()) {
    return {};
  }
  if (str.front() == ' ') {
    s = str.find_first_not_of(' ');
  }
  if (s == std::string_view::npos) {
    return {};
  }
  if (str.back() == ' ') {
    e = str.find_last_not_of(' ');
  }
  return str.substr(s, e - s + 1);
}

std::vector<std::string_view> Split(std::string_view s, char delim,
                                    bool keep_empty) {
  return Split(s, std::string_view(&delim, 1), keep_empty);
}

std::vector<std::string_view> Split(std::string_view s, std::string_view delim,
                                    bool keep_empty) {
  std::vector<std::string_view> splited;
  if (s.empty()) {
    return splited;
  }
  auto current = s;
  FLARE_CHECK(!delim.empty());
  while (true) {
    auto pos = current.find(delim);
    if (pos != 0 || keep_empty) {
      splited.push_back(current.substr(0, pos));
    }  // Empty part otherwise.
    if (pos == std::string_view::npos) {
      break;
    }
    current = current.substr(pos + delim.size());
    if (current.empty()) {
      if (keep_empty) {
        splited.push_back("");
      }
      break;
    }
  }
  return splited;
}

std::string Join(const std::vector<std::string_view>& parts,
                 std::string_view delim) {
  std::string result;
  JoinImpl(parts, delim, &result);
  return result;
}

std::string Join(const std::vector<std::string>& parts,
                 std::string_view delim) {
  std::string result;
  JoinImpl(parts, delim, &result);
  return result;
}

std::string Join(const std::initializer_list<std::string_view>& parts,
                 std::string_view delim) {
  std::string result;
  JoinImpl(parts, delim, &result);
  return result;
}

char ToUpper(char c) { return kUpperChars[c]; }
char ToLower(char c) { return kLowerChars[c]; }

void ToUpper(std::string* s) {
  for (auto&& c : *s) {
    c = ToUpper(c);
  }
}

void ToLower(std::string* s) {
  for (auto&& c : *s) {
    c = ToLower(c);
  }
}

std::string ToUpper(std::string_view s) {
  std::string result;
  result.reserve(s.size());
  for (auto&& e : s) {
    result.push_back(ToUpper(e));
  }
  return result;
}

std::string ToLower(std::string_view s) {
  std::string result;
  result.reserve(s.size());
  for (auto&& e : s) {
    result.push_back(ToLower(e));
  }
  return result;
}

bool IEquals(std::string_view first, std::string_view second) {
  if (first.size() != second.size()) {
    return false;
  }
  for (std::size_t index = 0; index != first.size(); ++index) {
    if (ToLower(first[index]) != ToLower(second[index])) {
      return false;
    }
  }
  return true;
}

std::optional<bool> TryParseTraits<bool>::TryParse(
    std::string_view s, bool recognizes_alphabet_symbol, bool ignore_case) {
  if (auto num_opt = flare::TryParse<int>(s); num_opt) {
    if (*num_opt == 0) {
      return false;
    } else if (*num_opt == 1) {
      return true;
    }
    return std::nullopt;
  }
  if (IEquals(s, "y") || IEquals(s, "yes") || IEquals(s, "true")) {
    return true;
  } else if (IEquals(s, "n") || IEquals(s, "no") || IEquals(s, "false")) {
    return false;
  }
  return std::nullopt;
}

template <class T>
std::optional<T>
TryParseTraits<T, std::enable_if_t<std::is_integral_v<T>>>::TryParse(
    std::string_view s, int base) {
  T value;
  auto [ptr, ec] = std::from_chars(s.begin(), s.end(), value, base);
  if (ec != std::errc() || ptr != s.end()) {
    return {};
  }
  return value;
}

template <class T>
std::optional<T>
TryParseTraits<T, std::enable_if_t<std::is_floating_point_v<T>>>::TryParse(
    std::string_view s) {
  // For floating point types, there's no definitive limit on its length, so we
  // optimize for most common case and fallback to heap allocation for the rest.
  char temp_buffer[129];
  std::string heap_buffer;
  const char* ptr;
  if (FLARE_LIKELY(s.size() < 128)) {
    ptr = temp_buffer;
    memcpy(temp_buffer, s.data(), s.size());
    temp_buffer[s.size()] = 0;
  } else {
    heap_buffer.assign(s.data(), s.size());
    ptr = heap_buffer.data();
  }

  // We cannot use the same trick as `TryParse<integral>` here, as casting
  // between floating point is lossy.
  if (std::is_same_v<T, float>) {
    return TryParseImpl<float>(&strtof, ptr, {-HUGE_VALF, HUGE_VALF});
  } else if (std::is_same_v<T, double>) {
    return TryParseImpl<double>(&strtod, ptr, {-HUGE_VAL, HUGE_VAL});
  } else if (std::is_same_v<T, long double>) {
    return TryParseImpl<long double>(&strtold, ptr, {-HUGE_VALL, HUGE_VALL});
  }
  FLARE_CHECK(0);
}

#define INSTANTIATE_TRY_PARSE_TRAITS(type) template struct TryParseTraits<type>;

INSTANTIATE_TRY_PARSE_TRAITS(char);                // NOLINT
INSTANTIATE_TRY_PARSE_TRAITS(signed char);         // NOLINT
INSTANTIATE_TRY_PARSE_TRAITS(signed short);        // NOLINT
INSTANTIATE_TRY_PARSE_TRAITS(signed int);          // NOLINT
INSTANTIATE_TRY_PARSE_TRAITS(signed long);         // NOLINT
INSTANTIATE_TRY_PARSE_TRAITS(signed long long);    // NOLINT
INSTANTIATE_TRY_PARSE_TRAITS(unsigned char);       // NOLINT
INSTANTIATE_TRY_PARSE_TRAITS(unsigned short);      // NOLINT
INSTANTIATE_TRY_PARSE_TRAITS(unsigned int);        // NOLINT
INSTANTIATE_TRY_PARSE_TRAITS(unsigned long);       // NOLINT
INSTANTIATE_TRY_PARSE_TRAITS(unsigned long long);  // NOLINT
INSTANTIATE_TRY_PARSE_TRAITS(float);
INSTANTIATE_TRY_PARSE_TRAITS(double);
INSTANTIATE_TRY_PARSE_TRAITS(long double);

}  // namespace flare
