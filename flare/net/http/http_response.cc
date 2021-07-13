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

#include "flare/net/http/http_response.h"

#include <string>

#include "flare/base/enum.h"
#include "flare/base/internal/early_init.h"
#include "flare/base/likely.h"
#include "flare/net/http/types.h"

using namespace std::literals;

namespace flare {

namespace {

static constexpr std::array<std::string_view, 600> kStatusCodeWithDesc = [] {
  std::array<std::string_view, 600> descs;
  descs[100] = "100 Continue"sv;
  descs[101] = "101 Switching Protocols"sv;
  descs[103] = "103 Early Hints"sv;
  descs[200] = "200 OK"sv;
  descs[201] = "201 Created"sv;
  descs[202] = "202 Accepted"sv;
  descs[203] = "203 Non-Authoritative Information"sv;
  descs[204] = "204 No Content"sv;
  descs[205] = "205 Reset Content"sv;
  descs[206] = "206 Partial Content"sv;
  descs[300] = "300 Multiple Choices"sv;
  descs[301] = "301 Moved Permanently"sv;
  descs[302] = "302 Found"sv;
  descs[303] = "303 See Other"sv;
  descs[304] = "304 Not Modified"sv;
  descs[307] = "307 Temporary Redirect"sv;
  descs[308] = "308 Permanent Redirect"sv;
  descs[400] = "400 Bad Request"sv;
  descs[401] = "401 Unauthorized"sv;
  descs[402] = "402 Payment Required"sv;
  descs[403] = "403 Forbidden"sv;
  descs[404] = "404 Not Found"sv;
  descs[405] = "405 Method Not Allowed"sv;
  descs[406] = "406 Not Acceptable"sv;
  descs[407] = "407 Proxy Authentication Required"sv;
  descs[408] = "408 Request Timeout"sv;
  descs[409] = "409 Conflict"sv;
  descs[410] = "410 Gone"sv;
  descs[411] = "411 Length Required"sv;
  descs[412] = "412 Precondition Failed"sv;
  descs[413] = "413 Payload Too Large"sv;
  descs[414] = "414 URI Too Long"sv;
  descs[415] = "415 Unsupported Media Type"sv;
  descs[416] = "416 Range Not Satisfiable"sv;
  descs[417] = "417 Expectation Failed"sv;
  descs[418] = "418 I'm a teapot"sv;
  descs[422] = "422 Unprocessable Entity"sv;
  descs[425] = "425 Too Early"sv;
  descs[426] = "426 Upgrade Required"sv;
  descs[428] = "428 Precondition Required"sv;
  descs[429] = "429 Too Many Requests"sv;
  descs[431] = "431 Request Header Fields Too Large"sv;
  descs[451] = "451 Unavailable For Legal Reasons"sv;
  descs[500] = "500 Internal Server Error"sv;
  descs[501] = "501 Not Implemented"sv;
  descs[502] = "502 Bad Gateway"sv;
  descs[503] = "503 Service Unavailable"sv;
  descs[504] = "504 Gateway Timeout"sv;
  descs[505] = "505 HTTP Version Not Supported"sv;
  descs[506] = "506 Variant Also Negotiates"sv;
  descs[507] = "507 Insufficient Storage"sv;
  descs[508] = "508 Loop Detected"sv;
  descs[510] = "510 Not Extended"sv;
  descs[511] = "511 Network Authentication Required"sv;
  return descs;
}();

}  // namespace

void HttpResponse::clear() noexcept {
  HttpMessage::clear();
  status_ = HttpStatus::OK;
}

std::string HttpResponse::ToString() const {
  std::string result;

  result += Format("{} {} {}\r\n", ToStringView(version()),
                   underlying_value(status_), ToStringView(status_));
  for (auto&& [k, v] : *headers()) {
    result += Format("{}: {}\r\n", k, v);
  }
  result += "\r\n";
  if (auto ptr = noncontiguous_body()) {
    result += FlattenSlow(*ptr);
  } else {
    result += *body();
  }
  return result;
}

void GenerateDefaultResponsePage(HttpStatus status, HttpResponse* response,
                                 std::string_view title,
                                 std::string_view body) {
  response->set_status(status);
  response->headers()->Append("Content-Type", "text/html");
  std::string html_body;
  html_body.assign("<html>\n<head>\n<title>");
  if (!title.empty()) {
    html_body.append(title);
  } else {
    html_body +=
        fmt::format("HTTP {}", http::GetStatusCodeWithDescString(status));
  }
  html_body.append("</title>\n</head>\n");
  html_body.append("<body>");
  if (!body.empty()) {
    html_body.append(body);
  } else {
    // TODO(luobogao): Use a more meaningful description here.
    html_body.append(http::GetStatusCodeWithDescString(status));
  }
  html_body.append("</body>\n</html>\n");
  response->set_body(html_body);
}

namespace http {

std::string_view GetStatusCodeWithDescString(HttpStatus status) noexcept {
  auto v = underlying_value(status);
  if (FLARE_UNLIKELY(v < 0 || v > std::size(kStatusCodeWithDesc))) {
    return internal::EarlyInitConstant<std::string_view>();
  }
  return kStatusCodeWithDesc[v];
}

}  // namespace http

}  // namespace flare
