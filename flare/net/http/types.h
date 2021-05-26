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

#ifndef FLARE_NET_HTTP_TYPES_H_
#define FLARE_NET_HTTP_TYPES_H_

#include <string_view>

#include "flare/base/string.h"

namespace flare {

enum class HttpVersion { Unspecified, V_1_0, V_1_1, V_2, V_3 };

enum class HttpMethod {
  Unspecified,  // Used as a placeholder. Do not use it.
  Head,
  Get,
  Post,
  Put,
  Delete,
  Options,
  Trace,
  Connect,
  Patch
};

// @sa: https://developer.mozilla.org/en-US/docs/Web/HTTP/Status
enum class HttpStatus {  // `HttpStatusCode`?
  Continue = 100,
  SwitchingProtocols = 101,
  EarlyHints = 103,
  OK = 200,
  Created = 201,
  Accepted = 202,
  NonAuthoritativeInformation = 203,
  NoContent = 204,
  ResetContent = 205,
  PartialContent = 206,
  MultipleChoices = 300,
  MovedPermanently = 301,
  Found = 302,
  SeeOther = 303,
  NotModified = 304,
  TemporaryRedirect = 307,
  PermanentRedirect = 308,
  BadRequest = 400,
  Unauthorized = 401,
  PaymentRequired = 402,
  Forbidden = 403,
  NotFound = 404,
  MethodNotAllowed = 405,
  NotAcceptable = 406,
  ProxyAuthenticationRequired = 407,
  RequestTimeout = 408,
  Conflict = 409,
  Gone = 410,
  LengthRequired = 411,
  PreconditionFailed = 412,
  PayloadTooLarge = 413,
  URITooLong = 414,
  UnsupportedMediaType = 415,
  RangeNotSatisfiable = 416,
  ExpectationFailed = 417,
  ImATeapot = 418,  // No you're not.
  UnprocessableEntity = 422,
  TooEarly = 425,
  UpgradeRequired = 426,
  PreconditionRequired = 428,
  TooManyRequests = 429,
  RequestHeaderFieldsTooLarge = 431,
  UnavailableForLegalReasons = 451,
  InternalServerError = 500,
  NotImplemented = 501,
  BadGateway = 502,
  ServiceUnavailable = 503,
  GatewayTimeout = 504,
  HTTPVersionNotSupported = 505,
  VariantAlsoNegotiates = 506,
  InsufficientStorage = 507,
  LoopDetected = 508,
  NotExtended = 510,
  NetworkAuthenticationRequired = 511
};

const std::string_view& ToStringView(HttpVersion method) noexcept;
const std::string_view& ToStringView(HttpMethod method) noexcept;
const std::string_view& ToStringView(HttpStatus status) noexcept;

template <>
struct TryParseTraits<HttpMethod> {
  static std::optional<HttpMethod> TryParse(const std::string_view& s);
};

}  // namespace flare

#endif  // FLARE_NET_HTTP_TYPES_H_
