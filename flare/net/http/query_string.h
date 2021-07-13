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

#ifndef FLARE_NET_HTTP_QUERY_STRING_H_
#define FLARE_NET_HTTP_QUERY_STRING_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "flare/base/string.h"

namespace flare {

template <class T, class>
struct TryParseTraits;

class HttpRequest;

// Represents a query string.
//
// This class treat `+` (plus sign) specially and decode it (if any) to
// whitespace.
//
// https://www.w3.org/Addressing/URL/uri-spec.txt:
//
// > Within the query string, the plus sign is reserved as shorthand notation
// > for a space.  Therefore, real plus signs must be encoded. [...]
class QueryString {
 public:
  QueryString() = default;

  // Get value of the first occurrence of the given key, or `std::nullopt` if
  // none.
  std::optional<std::string_view> TryGet(std::string_view key) const noexcept;

  // Same as `TryGet` except that `std::nullopt` is returned on conversion
  // failure .
  template <class T>
  std::optional<T> TryGet(std::string_view key) const {
    auto value = TryGet(key);
    return value ? TryParse<T>(*value) : std::nullopt;
  }

  // Get all occurrences of the given key.
  std::vector<std::string_view> TryGetMultiple(std::string_view key) const;

  // For iterating through KV-pairs.
  auto begin() const noexcept { return pairs_.begin(); }
  auto end() const noexcept { return pairs_.end(); }

  // STL-alike accessors.
  auto&& at(std::size_t index) const noexcept { return pairs_.at(index); }
  auto&& operator[](std::size_t index) const noexcept { return pairs_[index]; }
  bool empty() const noexcept { return pairs_.empty(); }
  std::size_t size() const noexcept { return pairs_.size(); }

  // Get strings representation of this object.
  std::string ToString() const { return original_; }

 private:
  friend struct TryParseTraits<QueryString, void>;

  explicit QueryString(std::string original,
                       std::vector<std::pair<std::string, std::string>> pairs)
      : original_(std::move(original)), pairs_(std::move(pairs)) {}

 private:
  std::string original_;
  std::vector<std::pair<std::string, std::string>> pairs_;
};

// TODO(luobogao): QueryStringBuilder

// Parse `QueryString` from its string representation.
//
// Defined implicitly in `flare/base/string.h`.
//
// std::optional<QueryString> TryParse<QueryString>(std::string_view);

// This method allows you to parse query string from URI.
std::optional<QueryString> TryParseQueryStringFromUri(const std::string& uri);

// This method is provided to simplify parsing query string from HTTP request.
// It parses both URI and HTTP body (if `Content-Type` indicates we should do
// so).
//
// If `force_parse_body` is set, HTTP body is parsed as if it's encoded as
// `application/x-www-form-urlencoded` regardless of `Content-Type`.
//
// NOTE: We might deprecate this method once `HttpParams` is implemented.
std::optional<QueryString> TryParseQueryStringFromHttpRequest(
    const HttpRequest& req, bool force_parse_body = false);

//////////////////////////////////
// Implementation goes below.   //
//////////////////////////////////

template <>
struct TryParseTraits<QueryString, void> {
  static std::optional<QueryString> TryParse(std::string_view s);
};

}  // namespace flare

#endif  // FLARE_NET_HTTP_QUERY_STRING_H_
