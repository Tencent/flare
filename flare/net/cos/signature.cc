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

#include "flare/net/cos/signature.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "flare/base/chrono.h"
#include "flare/base/crypto/sha.h"
#include "flare/base/encoding/hex.h"
#include "flare/base/encoding/percent.h"
#include "flare/base/logging.h"
#include "flare/base/string.h"

namespace flare::cos {

namespace {

// Implementation below is rather slow, let's see if we need to optimize them
// when used in real-world workload.

std::string MakeKeyTime() {
  auto now = ReadUnixTimestamp();
  // 600 seconds should be far more than enough.
  return Format("{};{}", now, now + 600);
}

std::string DecodePctMustSucceed(std::string_view str) {
  auto decoded = DecodePercent(str);
  FLARE_CHECK(decoded, "Invalid pct-encoded string: {}", str);
  return std::move(*decoded);
}

std::pair<std::string, std::string> GetPathAndQueryFromUri(
    std::string_view uri) {
  auto pos1 = uri.find_last_of('/');
  if (pos1 == std::string_view::npos) {
    return {"/", ""};  // Both are empty.
  }
  auto path = uri.substr(pos1);  // Backslash if preserved.
  auto pos2 = path.find_last_of('?');
  if (pos2 == std::string_view::npos) {
    return {DecodePctMustSucceed(path), ""};
  }
  return {DecodePctMustSucceed(path.substr(0, pos2)),
          std::string(path.substr(pos2 + 1))};
}

std::vector<std::pair<std::string, std::string>> ParseQueryString(
    std::string_view query_str) {
  std::vector<std::pair<std::string, std::string>> result;
  auto split = Split(query_str, "&");
  for (auto&& e : split) {
    auto pos = e.find('=');
    if (pos != std::string::npos) {
      result.emplace_back(DecodePctMustSucceed(e.substr(0, pos)),
                          DecodePctMustSucceed(e.substr(pos + 1)));
    } else {
      result.emplace_back(DecodePctMustSucceed(e), "");
    }
  }
  return result;
}

std::vector<std::pair<std::string, std::string>> ParseHeaders(
    const std::vector<std::string>& headers) {
  std::vector<std::pair<std::string, std::string>> result;
  for (auto&& e : headers) {
    auto pos = e.find(':');
    if (pos != std::string::npos) {
      result.emplace_back(e.substr(0, pos), Trim(e.substr(pos + 1)));
    } else {
      result.emplace_back(e, "");
    }
  }
  return result;
}

std::vector<std::pair<std::string, std::string>> ToPctEncodedLowercaseAndSorted(
    const std::vector<std::pair<std::string, std::string>>& from) {
  std::vector<std::pair<std::string, std::string>> result;
  for (auto&& [k, v] : from) {
    result.emplace_back(EncodePercent(k), EncodePercent(v));
    ToLower(&result.back().first);
  }
  std::sort(result.begin(), result.end());
  return result;
}

std::pair<std::string, std::string> MakeKeyStringAndKVStrings(
    const std::vector<std::pair<std::string, std::string>>&
        pct_encoded_lowercased_and_sorted) {
  std::string result1, result2;
  for (auto&& [k, v] : pct_encoded_lowercased_and_sorted) {
    result1 += k + ";";
    result2 += Format("{}={}&", k, v);
  }
  if (!result1.empty()) {
    result1.pop_back();
    result2.pop_back();
  }
  return std::pair(result1, result2);
}

}  // namespace

std::string GenerateCosAuthString(const std::string& secret_id,
                                  const std::string& secret_key,
                                  HttpMethod method, const std::string& uri,
                                  const std::vector<std::string>& hdrs,
                                  const std::string& key_time) {
  auto&& [path, query] = GetPathAndQueryFromUri(uri);
  auto queries = ParseQueryString(query);
  auto lower_queries = ToPctEncodedLowercaseAndSorted(queries);
  auto lower_hdrs = ToPctEncodedLowercaseAndSorted(ParseHeaders(hdrs));
  auto timestamp = key_time.empty() ? MakeKeyTime() : key_time;
  auto sign_key = EncodeHex(HmacSha1(secret_key, timestamp));
  auto&& [uri_param_list, http_params] =
      MakeKeyStringAndKVStrings(lower_queries);
  auto&& [hdr_list, http_hdrs] = MakeKeyStringAndKVStrings(lower_hdrs);
  auto http_str = Format("{}\n{}\n{}\n{}\n", ToLower(ToStringView(method)),
                         path, http_params, http_hdrs);
  auto str_to_sign(
      Format("sha1\n{}\n{}\n", timestamp, EncodeHex(Sha1(http_str))));
  auto signature = HmacSha1(sign_key, str_to_sign);
  auto auth_str = Format(
      "q-sign-algorithm=sha1&q-ak={}&q-sign-time={}&q-key-time={}&q-header-"
      "list={}&q-url-param-list={}&q-signature={}",
      secret_id, timestamp, timestamp, hdr_list, uri_param_list,
      EncodeHex(signature));
  return auth_str;
}

}  // namespace flare::cos
