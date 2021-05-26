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

#include "flare/rpc/protocol/http/buffer_io.h"

#include <cstdlib>

#include <algorithm>
#include <forward_list>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "thirdparty/gflags/gflags.h"

#include "flare/base/buffer.h"
#include "flare/base/string.h"
#include "flare/net/http/http_headers.h"
#include "flare/net/http/http_request.h"
#include "flare/net/http/http_response.h"

using namespace std::literals;

// @sa: https://stackoverflow.com/a/8623061
DEFINE_int32(flare_http_max_header_size, 8192,
             "Maximum size of total size of HTTP headers.");

namespace flare::http {

namespace {

inline void WriteHeader(const HttpHeaders& headers,
                        NoncontiguousBufferBuilder* builder) {
  for (auto&& [k, v] : headers) {
    builder->Append(k, ": "sv, v, "\r\n"sv);
  }
}

template <class T>
inline std::string_view View(const T& block) {
  return std::string_view(block.data(), block.size());
}

constexpr auto kEndOfHeaderMarker = "\r\n\r\n"sv;

inline bool EasyDetectHttp(const void* data, std::size_t length) {
  static constexpr std::string_view kLeadingWords[] = {
      "HTTP/1.1"sv, "HEAD"sv,
      // It `\t` also considered whitespace here?
      "GET "sv, "POST"sv, "PUT "sv, "DELETE"sv, "TRACE"sv, "CONNECT"sv,
      "PATCH"sv};
  static constexpr std::size_t kMinLength = [&] {
    std::size_t result = 0;
    for (auto&& e : kLeadingWords) {
      result = std::max(result, e.size());
    }
    return result;
  }();
  if (length < kMinLength) {
    return false;
  }

  for (auto&& e : kLeadingWords) {
    if (memcmp(data, e.data(), 4) == 0) {
      return true;
    }
  }
  return false;
}

std::size_t DetermineHeaderSizeFast(const char* ptr, std::size_t limit) {
  auto ep = ptr + limit;
  auto p = ptr + 1;
  while (true) {
    // First '\n' in '\r\n\r\n'.
    p = static_cast<char*>(memchr(const_cast<char*>(p), '\n', ep - p));
    if (FLARE_UNLIKELY(p == nullptr || ep - p < 3)) {
      return -1;
    }
    if (p[2] == '\n') {
      FLARE_CHECK_GE(p - 1, ptr);
      if (FLARE_LIKELY(std::string_view(p - 1, 4) == kEndOfHeaderMarker)) {
        return (p + 3) - ptr;
      } else {
        // Otherwise it's malformed, but how should we handle it?
        return -1;
      }
    }
    ++p;  // Keep looping otherwise.
  }
}

}  // namespace

ReadStatus ReadHeader(const NoncontiguousBuffer& buffer, HeaderBlock* header) {
  // The header shouldn't be too large. If we're going to find it, it's likely
  // to be contiguous physically.
  auto first_block = buffer.FirstContiguous();
  auto size = DetermineHeaderSizeFast(first_block.data(), first_block.size());
  if (FLARE_LIKELY(size != -1)) {
    // Do NOT use `std::make_unique<char[]>(size)` here, it will zero-initialize
    // the array, which is both wasteful, and *rather* slow. (I suspect the
    // slowness is a QoI issue of GCC 8.2, it zeros elements one-by-one, whilst
    // is should use memset instead.). (No, `std::make_unique_for_overwrite` is
    // not available at the time of writing, unfortunately.)
    header->first = std::unique_ptr<char[]>(new char[size]);
    header->second = size;
    memcpy(header->first.get(), first_block.data(), size);
  } else {
    if (first_block.size() >= 10 &&
        !EasyDetectHttp(first_block.data(), first_block.size())) {
      return ReadStatus::UnexpectedFormat;
    }
    if (buffer.ByteSize() == first_block.size()) {
      return first_block.size() < FLAGS_flare_http_max_header_size
                 ? ReadStatus::NoEnoughData
                 : ReadStatus::Error;
    }
    auto slow_buffer = FlattenSlowUntil(buffer, kEndOfHeaderMarker);
    if (!EndsWith(slow_buffer, kEndOfHeaderMarker)) {
      return slow_buffer.size() < FLAGS_flare_http_max_header_size
                 ? ReadStatus::NoEnoughData
                 : ReadStatus::Error;
    }
    auto size = slow_buffer.size();  // Indeed it's "slow" buffer.
    header->first = std::unique_ptr<char[]>(new char[size]);  // Perf. reasons.
    header->second = size;
    memcpy(header->first.get(), slow_buffer.data(), size);
  }
  if (FLARE_UNLIKELY(!EasyDetectHttp(header->first.get(), header->second))) {
    return ReadStatus::UnexpectedFormat;
  }
  return ReadStatus::OK;
}

std::string_view ReadFieldFromRawBytes(const std::string_view& view,
                                       const std::string_view& key) {
  // https://tools.ietf.org/html/rfc7230#section-3.5
  //
  // Although the line terminator for the start-line and header fields is the
  // sequence CRLF, a recipient MAY recognize a single LF as a line terminator
  // and ignore any preceding CR.
  std::size_t pos = 0;
  while (true) {
    auto field = view.substr(pos);
    // Field name is not allowed to be prefixed or suffixed with whitespaces.
    //
    // message-header = field-name ":" [ field-value ]
    // field-name     = token
    // field-value    = *( field-content | LWS )
    if (IEquals(field.substr(0, key.size()), key)) {
      if (FLARE_UNLIKELY(field.size() == key.size())) {
        FLARE_LOG_WARNING_EVERY_SECOND("Invalid HTTP header? Read [{}].",
                                       field);
        return ""sv;
      }
      if (FLARE_LIKELY(field[key.size()] == ':')) {
        // Here you go.
        auto last_pos = field.find('\r');
        // This can't be, `header` produced by `CutHeader` should always ends
        // with "\r\n\r\n".
        FLARE_CHECK_NE(last_pos, std::string_view::npos);
        // Unless `name` itself contains `\n`, which is a programming error (as
        // `name` is hardcoded by the caller), this assertion can't fire.
        FLARE_CHECK_GT(last_pos, key.size() + 1);
        return Trim(field.substr(key.size() + 1, last_pos - (key.size() + 1)));
      }
    }
    pos = view.find('\n', pos + 1);
    if (pos == std::string_view::npos) {
      return ""sv;  // Not found.
    }
    ++pos;  // At most `view.size()`, `view.substr(pos)` won't raise.
  }
}

bool ParseMessagePartial(HeaderBlock&& storage, std::string_view* start_line,
                         HttpHeaders* headers) {
  auto storage_ref = headers->RetrieveHeaderStorage(std::move(storage));
  std::vector<std::pair<std::string_view, std::string_view>> fields;
  fields.reserve(8);

  FLARE_CHECK(EndsWith(storage_ref, kEndOfHeaderMarker));  // `CutHeader` bugs.
  auto view = storage_ref.substr(0, storage_ref.size() - 2);
  auto pos = view.find('\n');
  FLARE_CHECK_NE(pos, std::string_view::npos);  // `CutHeader` bugs otherwise.
  if (FLARE_UNLIKELY(pos == 1)) {
    FLARE_LOG_WARNING_EVERY_SECOND("Invalid HTTP header? Read [{}].", view);
    return false;
  }
  *start_line = view.substr(0, pos - 1);
  view = view.substr(pos + 1);
  while (!view.empty()) {
    auto pos = view.find('\n');
    FLARE_CHECK_NE(pos, std::string_view::npos);  // Or `CutHeaders` bugs.
    if (FLARE_UNLIKELY(pos < 2 || view[pos - 1] != '\r')) {
      FLARE_LOG_WARNING_EVERY_SECOND("Invalid HTTP header?");
      return false;
    }
    auto current = view.substr(0, pos - 1);
    // Field name is not allowed to be prefixed or suffixed with whitespaces.
    //
    // message-header = field-name ":" [ field-value ]
    // field-name     = token
    // field-value    = *( field-content | LWS )
    auto pcolon = current.find(':');
    if (pcolon == std::string_view::npos) {
      FLARE_LOG_WARNING_EVERY_SECOND("Invalid message-header? Read [{}].",
                                     current);
      return false;
    }
    auto name = current.substr(0, pcolon);
    auto value = current.substr(pcolon + 1);
    if (name.empty()) {
      FLARE_LOG_WARNING_EVERY_SECOND("Empty field-name? Read [{}].", current);
      return false;
    }
    fields.emplace_back(name, Trim(value));
    view = view.substr(pos + 1);
  }

  headers->RetrieveFields(std::move(fields));
  return true;
}

bool ParseRequestStartLine(const std::string_view& s, HttpVersion* version,
                           HttpMethod* method, std::string_view* uri) {
  // TODO(luobogao): Detect HTTP version.
  auto splitted = Split(s, " ");
  if (splitted.size() != 3) {
    FLARE_LOG_WARNING_EVERY_SECOND("Invalid HTTP request Start-Line: {}", s);
    return false;
  }
  if (FLARE_LIKELY(splitted[2] == "HTTP/1.1")) {
    *version = HttpVersion::V_1_1;
  } else if (splitted[2] == "HTTP/1.0") {
    *version = HttpVersion::V_1_0;
  } else {
    FLARE_LOG_WARNING_EVERY_SECOND("Invalid HTTP version [{}].", splitted[2]);
    return false;
  }
  if (auto opt = TryParse<HttpMethod>(splitted[0]); !opt) {
    FLARE_LOG_WARNING_EVERY_SECOND("Invalid HTTP request Start-Line: {}", s);
    return false;
  } else {
    *method = *opt;
  }
  *uri = splitted[1];
  return true;
}

bool ParseResponseStartLine(const std::string_view& s, HttpStatus* code) {
  // TODO(luobogao): Detect HTTP version.
  auto splitted = Split(s, " ");
  if (splitted.size() < 2) {  // HTTP/1.1 301 Moved Permanently
    FLARE_LOG_WARNING_EVERY_SECOND("Invalid HTTP response Start-Line: {}", s);
    return false;
  }
  if (auto opt = TryParse<int>(splitted[1]); !opt) {
    FLARE_LOG_WARNING_EVERY_SECOND("Invalid HTTP response Start-Line: {}", s);
    return false;
  } else {
    *code = static_cast<HttpStatus>(*opt);
  }
  return true;
}

void WriteMessage(const HttpRequest& msg, NoncontiguousBufferBuilder* builder) {
  // @sa: https://www.w3.org/Protocols/rfc2616/rfc2616-sec5.html

  // Request-Line   = Method SP Request-URI SP HTTP-Version CRLF
  builder->Append(ToStringView(msg.method()), " "sv, msg.uri(), " "sv,
                  ToStringView(msg.version()),
                  "\r\n"sv);             // Start-Line.
  WriteHeader(*msg.headers(), builder);  // Header fields.
  builder->Append("\r\n"sv);
  if (msg.noncontiguous_body()) {
    builder->Append(*msg.noncontiguous_body());
  } else {
    builder->Append(*msg.body());
  }
}

void WriteMessage(const HttpResponse& msg,
                  NoncontiguousBufferBuilder* builder) {
  // @sa: https://www.w3.org/Protocols/rfc2616/rfc2616-sec6.html

  // Status-Line = HTTP-Version SP Status-Code SP Reason-Phrase CRLF
  builder->Append(ToStringView(msg.version()), " "sv,
                  GetStatusCodeWithDescString(msg.status()),
                  "\r\n"sv);             // Start-Line.
  WriteHeader(*msg.headers(), builder);  // Header fields.
  builder->Append("\r\n"sv);
  if (msg.noncontiguous_body()) {
    builder->Append(*msg.noncontiguous_body());
  } else {
    builder->Append(*msg.body());
  }
}

}  // namespace flare::http
