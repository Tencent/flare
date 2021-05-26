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

#include "flare/net/http/query_string.h"

#include "flare/base/encoding/percent.h"
#include "flare/base/net/uri.h"
#include "flare/base/string.h"
#include "flare/net/http/http_request.h"

namespace flare {

std::optional<std::string_view> QueryString::TryGet(
    const std::string_view& key) const noexcept {
  for (auto&& [k, v] : pairs_) {
    // Keys are case sensitive. Don't call `IEquals` here.
    //
    // https://tools.ietf.org/html/rfc3986
    //
    // > The other generic syntax components are assumed to be case-sensitive
    // > unless specifically defined otherwise by the scheme [...]
    if (k == key) {
      return v;
    }
  }
  return std::nullopt;
}

std::vector<std::string_view> QueryString::TryGetMultiple(
    const std::string_view& key) const {
  std::vector<std::string_view> result;
  for (auto&& [k, v] : pairs_) {
    if (k == key) {
      result.push_back(v);
    }
  }
  return result;
}

std::optional<QueryString> TryParseTraits<QueryString, void>::TryParse(
    const std::string_view& s) {
  std::vector<std::pair<std::string, std::string>> pairs;

  auto parts = Split(s, "&");
  for (auto&& e : parts) {
    auto sep = e.find('=');
    if (sep != std::string_view::npos) {
      auto key = DecodePercent(e.substr(0, sep));
      auto value = DecodePercent(e.substr(sep + 1));
      if (!key || !value) {
        return std::nullopt;
      }
      pairs.emplace_back(*key, *value);
    } else {
      auto decoded = DecodePercent(e, true);
      if (!decoded) {
        return std::nullopt;
      }
      pairs.emplace_back(*decoded, std::string_view());
    }
  }
  return QueryString(std::string(s), std::move(pairs));
}

std::optional<QueryString> TryParseQueryStringFromUri(const std::string& uri) {
  auto uri_opt = TryParse<Uri>(uri);
  if (!uri_opt) {
    return std::nullopt;
  }
  return TryParse<QueryString>(uri_opt->query());
}

std::optional<QueryString> TryParseQueryStringFromHttpRequest(
    const HttpRequest& req, bool force_parse_body) {
  auto uri_opt = TryParse<Uri>(req.uri());
  if (!uri_opt) {
    return std::nullopt;
  }

  // We concatenate queries in both URI and body before parsing it.
  std::string all_queries(uri_opt->query());

  // `charset=utf-8` does not seem to be allowed for this `Content-Type`, so
  // don't bother handling it.
  if (force_parse_body || req.headers()->TryGet("Content-Type") ==
                              "application/x-www-form-urlencoded") {
    if (!all_queries.empty()) {
      all_queries += "&";
    }
    if (auto body = req.noncontiguous_body()) {
      all_queries += FlattenSlow(*body);
    } else {
      all_queries += *req.body();
    }
  }
  return TryParse<QueryString>(all_queries);
}

}  // namespace flare
