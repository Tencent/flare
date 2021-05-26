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

#include "flare/net/http/types.h"

#include <array>
#include <iterator>
#include <string_view>
#include <unordered_map>

#include "flare/base/enum.h"
#include "flare/base/internal/early_init.h"
#include "flare/base/likely.h"

using namespace std::literals;

namespace flare {

namespace {

constexpr std::array<std::string_view, 600> kStatusCodes = [] {
  std::array<std::string_view, 600> descs;
  descs[100] = "Continue"sv;
  descs[101] = "Switching Protocols"sv;
  descs[103] = "Early Hints"sv;
  descs[200] = "OK"sv;
  descs[201] = "Created"sv;
  descs[202] = "Accepted"sv;
  descs[203] = "Non-Authoritative Information"sv;
  descs[204] = "No Content"sv;
  descs[205] = "Reset Content"sv;
  descs[206] = "Partial Content"sv;
  descs[300] = "Multiple Choices"sv;
  descs[301] = "Moved Permanently"sv;
  descs[302] = "Found"sv;
  descs[303] = "See Other"sv;
  descs[304] = "Not Modified"sv;
  descs[307] = "Temporary Redirect"sv;
  descs[308] = "Permanent Redirect"sv;
  descs[400] = "Bad Request"sv;
  descs[401] = "Unauthorized"sv;
  descs[402] = "Payment Required"sv;
  descs[403] = "Forbidden"sv;
  descs[404] = "Not Found"sv;
  descs[405] = "Method Not Allowed"sv;
  descs[406] = "Not Acceptable"sv;
  descs[407] = "Proxy Authentication Required"sv;
  descs[408] = "Request Timeout"sv;
  descs[409] = "Conflict"sv;
  descs[410] = "Gone"sv;
  descs[411] = "Length Required"sv;
  descs[412] = "Precondition Failed"sv;
  descs[413] = "Payload Too Large"sv;
  descs[414] = "URI Too Long"sv;
  descs[415] = "Unsupported Media Type"sv;
  descs[416] = "Range Not Satisfiable"sv;
  descs[417] = "Expectation Failed"sv;
  descs[418] = "I'm a teapot"sv;
  descs[422] = "Unprocessable Entity"sv;
  descs[425] = "Too Early"sv;
  descs[426] = "Upgrade Required"sv;
  descs[428] = "Precondition Required"sv;
  descs[429] = "Too Many Requests"sv;
  descs[431] = "Request Header Fields Too Large"sv;
  descs[451] = "Unavailable For Legal Reasons"sv;
  descs[500] = "Internal Server Error"sv;
  descs[501] = "Not Implemented"sv;
  descs[502] = "Bad Gateway"sv;
  descs[503] = "Service Unavailable"sv;
  descs[504] = "Gateway Timeout"sv;
  descs[505] = "HTTP Version Not Supported"sv;
  descs[506] = "Variant Also Negotiates"sv;
  descs[507] = "Insufficient Storage"sv;
  descs[508] = "Loop Detected"sv;
  descs[510] = "Not Extended"sv;
  descs[511] = "Network Authentication Required"sv;
  return descs;
}();

constexpr std::string_view kStringizedVersion[] = {
    "(Unspecified)", "HTTP/1.0", "HTTP/1.1", "HTTP/2", "HTTP/3"};

constexpr std::string_view kStringifiedMethods[] = {
    /* Unspecified */ "UNSPECIFIED"sv,
    /* Head */ "HEAD"sv,
    /* Get */ "GET"sv,
    /* Post */ "POST"sv,
    /* Put */ "PUT"sv,
    /* Delete */ "DELETE"sv,
    /* Options */ "OPTIONS"sv,
    /* Trace */ "TRACE"sv,
    /* Connect */ "CONNECT"sv,
    /* Patch */ "PATCH"sv,
};

const std::unordered_map<std::string_view, HttpMethod> kMethodEnumerators = {
    {"UNSPECIFIED"sv, HttpMethod::Unspecified},
    {"HEAD"sv, HttpMethod::Head},
    {"GET"sv, HttpMethod::Get},
    {"POST"sv, HttpMethod::Post},
    {"PUT"sv, HttpMethod::Put},
    {"DELETE"sv, HttpMethod::Delete},
    {"TRACE"sv, HttpMethod::Trace},
    {"CONNECT"sv, HttpMethod::Connect},
    {"PATCH"sv, HttpMethod::Patch},
};

}  // namespace

const std::string_view& ToStringView(HttpStatus status) noexcept {
  auto v = underlying_value(status);
  if (FLARE_UNLIKELY(v < 0 || v > std::size(kStatusCodes))) {
    return internal::EarlyInitConstant<std::string_view>();
  }
  return kStatusCodes[v];
}

const std::string_view& ToStringView(HttpVersion version) noexcept {
  auto index = underlying_value(version);
  if (FLARE_UNLIKELY(index < 0 || index > std::size(kStringizedVersion))) {
    return internal::EarlyInitConstant<std::string_view>();
  }
  return kStringizedVersion[index];
}

const std::string_view& ToStringView(HttpMethod method) noexcept {
  auto v = underlying_value(method);
  if (FLARE_UNLIKELY(v < 0 || v > std::size(kStringifiedMethods))) {
    return internal::EarlyInitConstant<std::string_view>();
  }
  return kStringifiedMethods[v];
}

std::optional<HttpMethod> TryParseTraits<HttpMethod>::TryParse(
    const std::string_view& s) {
  auto iter = kMethodEnumerators.find(s);
  if (iter == kMethodEnumerators.end()) {
    return std::nullopt;
  }
  return iter->second;
}

}  // namespace flare
