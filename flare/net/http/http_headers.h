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

#ifndef FLARE_NET_HTTP_HTTP_HEADERS_H_
#define FLARE_NET_HTTP_HTTP_HEADERS_H_

#include <deque>
#include <forward_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "flare/base/buffer.h"
#include "flare/base/internal/case_insensitive_hash_map.h"
#include "flare/base/logging.h"
#include "flare/base/string.h"

namespace flare {

class HttpHeaders;

namespace http {

bool ParseMessagePartial(
    std::pair<std::unique_ptr<char[]>, std::size_t>&& storage,
    std::string_view* start_line, HttpHeaders* headers);

}  // namespace http

// Primarily used for parsing headers and hold the result.
//
// TDOO(luobogao): It's hard to optimize for both reader side (receiver / parser
// side) and write side (sender / builder side). However, we can provide two
// different implementation, each optimized for reader / writer side, and leave
// `HttpHeader` as a wrapper for those implementation. This way we can use an
// optimized-for-read implementation when the framework itself is generating
// HTTP message, and defaults to an optimized-for-write implementation when the
// user constructs an HTTP message.
class HttpHeaders {
  // https://tools.ietf.org/html/rfc7230#section-3.2.2
  //
  // > The order in which header fields with differing field names are received
  // > is not significant. [...]
  // >
  // > A sender MUST NOT generate multiple header fields with the same field
  // > name in a message unless either the entire field value for that header
  // > field is defined as a comma-separated list [i.e., #(values)] or the
  // > header field is a well-known exception (as noted below). [...]
  // >
  // > [...] The order in which header fields with the same field name are
  // > received is therefore significant to the interpretation of the combined
  // > field value; a proxy MUST NOT change the order of these field values when
  // > forwarding a message.
  //
  // Put simply, it's allowed to have several fields with same name, and in such
  // case, the order is significant. Therefore, we can't simply use a `std::map`
  // or `std::unordered_map` (or whatever `map`-alike container) to hold the
  // header fields.
  using NonowningFields =
      std::vector<std::pair<std::string_view, std::string_view>>;

 public:
  using iterator = NonowningFields::iterator;
  using const_iterator = NonowningFields::const_iterator;

  HttpHeaders() = default;
  HttpHeaders(HttpHeaders&&) = default;
  HttpHeaders& operator=(HttpHeaders&&) = default;
  HttpHeaders(const HttpHeaders&);
  HttpHeaders& operator=(const HttpHeaders&);

  iterator begin() noexcept { return fields_.begin(); }
  const_iterator begin() const noexcept { return fields_.begin(); }
  iterator end() noexcept { return fields_.end(); }
  const_iterator end() const noexcept { return fields_.end(); }

  bool contains(const std::string_view& key) const noexcept {
    return header_idx_.contains(key);
  }

  void clear() noexcept;

  // `key` is case-insensitive. Returns `nullptr` is returned if not found.
  //
  // > Each header field consists of a case-insensitive field name followed by a
  // > colon (":")
  std::optional<std::string_view> TryGet(
      const std::string_view& key) const noexcept {
    auto idx_opt = header_idx_.TryGet(key);
    return idx_opt ? std::make_optional(fields_[*idx_opt].second)
                   : std::nullopt;
  }

  template <class T>
  std::optional<T> TryGet(const std::string_view& key) const noexcept {
    auto p = TryGet(key);
    if (p) {
      return TryParse<T>(*p);
    }
    return std::nullopt;
  }

  // `key` is case-insensitive. Empty set is returned if not found.
  //
  // This one does not perform as well as `TryGet`, only if you suspect (or
  // believe) that there are multiple fields with the same name should you use
  // this one.
  std::vector<std::string_view> TryGetMultiple(
      const std::string_view& key) const noexcept;

  // Set a header field. if it exists, overwrite the header value.
  //
  // NOTE THAT THIS METHOD IS RATHER SLOW.
  void Set(std::string key, std::string value);

  // Append new field at the end.
  void Append(std::string key, std::string value);

  // Append a series of header fields.
  void Append(const std::initializer_list<
              std::pair<std::string_view, std::string_view>>& fields);

  // `key` is case-insensitive.
  // Always successfully remove the specified header.
  // Returns true if key originally exists in the header.
  bool Remove(const std::string_view& key) noexcept;

  // Primarily for debugging purpose. We do not use it for serializing due to
  // its performance penalty. (@sa: `HeaderWriter`)
  std::string ToString() const;

 private:
  friend bool http::ParseMessagePartial(
      std::pair<std::unique_ptr<char[]>, std::size_t>&& storage,
      std::string_view* start_line, HttpHeaders* headers);

  // Used by `ParseMessagePartial`.
  std::string_view RetrieveHeaderStorage(
      std::pair<std::unique_ptr<char[]>, std::size_t>&& s);
  void RetrieveFields(NonowningFields&& fields);

 private:
  // Except for what's added by the user, header name / values kept by
  // `fields_` are actually reference to this buffer.
  std::pair<std::unique_ptr<char[]>, std::size_t> header_block_;
  // For header fields insertion done by users, the strings are stored here.
  std::deque<std::string> owning_strs_2_;
  // For better lookup (which is likely to be done frequently if we're
  // parsing, instead of building, headers) performance, we build this map for
  // that purpose. Values are indices into `fields_`.
  internal::CaseInsensitiveHashMap<std::string_view, std::size_t> header_idx_;
  // Referencing either `buffer_` or `owning_strs_`.
  NonowningFields fields_;
};

}  // namespace flare

#endif  // FLARE_NET_HTTP_HTTP_HEADERS_H_
