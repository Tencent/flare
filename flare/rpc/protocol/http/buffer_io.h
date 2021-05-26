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

#ifndef FLARE_RPC_PROTOCOL_HTTP_BUFFER_IO_H_
#define FLARE_RPC_PROTOCOL_HTTP_BUFFER_IO_H_

#include <memory>
#include <utility>

#include "flare/base/buffer.h"

namespace flare {

enum class HttpVersion;
enum class HttpMethod;
enum class HttpStatus;
class HttpRequest;
class HttpResponse;
class HttpHeaders;

}  // namespace flare

namespace flare::http {

enum class ReadStatus { OK, NoEnoughData, UnexpectedFormat, Error };

using HeaderBlock = std::pair<std::unique_ptr<char[]>, std::size_t>;

// We don't expect header to be very large (and we set a upper bound size
// anyway). Therefore flatten it before parsing should boost overall
// performance.
//
// On successful return, `header` contains the following:
//
// - start-line
// - *(message-header CRLF)
// - CRLF
//
// (See https://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4 for the
// meaning of terms used here.)
//
// On `OK`, `buffer` is consumed.
// On `NoEnoughData` or `UnexpectedFormat`, `buffer` is left untouched.
// On `Error`, `buffer` is left in an INCONSISTENT state.
ReadStatus ReadHeader(const NoncontiguousBuffer& buffer, HeaderBlock* storage);

// Before parsing the header completely, we might need some fields more early
// than others ("Content-Length", "Transfer-Encoding" (for "chunked" encoding),
// to name a few). This method does a "dirty & quick" parse to read the header
// field.
std::string_view ReadFieldFromRawBytes(const std::string_view& view,
                                       const std::string_view& name);

// Parse start-line and header-fields from `buffer`, which should be cut by
// `ReadHeader`.
//
// `storage` is moved into `headers`.
//
// You should handle message body (if any) yourself.
bool ParseMessagePartial(HeaderBlock&& storage, std::string_view* start_line,
                         HttpHeaders* headers);

// Parse Start-Line.
bool ParseRequestStartLine(const std::string_view& s, HttpVersion* version,
                           HttpMethod* method, std::string_view* uri);
bool ParseResponseStartLine(const std::string_view& s, HttpStatus* code);

// Write entire message into buffer builder.
void WriteMessage(const HttpRequest& msg, NoncontiguousBufferBuilder* builder);
void WriteMessage(const HttpResponse& msg, NoncontiguousBufferBuilder* builder);

}  // namespace flare::http

#endif  // FLARE_RPC_PROTOCOL_HTTP_BUFFER_IO_H_
