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

#ifndef FLARE_BASE_ENCODING_PERCENT_H_
#define FLARE_BASE_ENCODING_PERCENT_H_

#include <optional>
#include <string>
#include <string_view>

#include "flare/base/internal/early_init.h"

namespace flare {

enum class PercentEncodingStyle {
  // Javascript `escape` is intentionally not supported. It's deprecated by the
  // ECMAScript standard.

  // Same as `encodeURIComponent` or `encodeURI`, depending on whether reserved
  // characters are going to be escaped.
  Ecma262 = 0,

  // Behavior specified in RFC3986.
  Rfc3986 = 1,

  // Behavior specified in RFC5987. Note that in this spec there is no such
  // thing as "reserved characters". Character is either escaped or not.
  //
  // Therefore `PercentEncodingOptions::escape_reserved` has no effect if this
  // style
  // is used.
  Rfc5987 = 2

  // PHP `urlrawencode`?
};

struct PercentEncodingOptions {
  // The most widely-used.
  PercentEncodingStyle style = PercentEncodingStyle::Rfc3986;

  // If set, reserved characters (if any, per `style`) are also escaped. (E.g.,
  // If `Ecma262` is chosen and this flag is not set, you'll get the same result
  // as of `encodeURI` in ECMAScript.)
  bool escape_reserved = true;
};

// Encode string as pct-encoded.
std::string EncodePercent(
    std::string_view from,
    const PercentEncodingOptions& options =
        internal::EarlyInitConstant<PercentEncodingOptions>());

// Decode pct-encoded string.
//
// If `decode_plus_sign_as_whitespace` is set, plus sign (`+`) is decoded as
// whitespace. This option is provided to decode things such as query string
// (some implementation uses a legacy encoding scheme and encodes whitespace as
// such).
std::optional<std::string> DecodePercent(
    std::string_view from, bool decode_plus_sign_as_whitespace = false);

void EncodePercent(std::string_view from, std::string* to,
                   const PercentEncodingOptions& options =
                       internal::EarlyInitConstant<PercentEncodingOptions>());
bool DecodePercent(std::string_view from, std::string* to,
                   bool decode_plus_sign_as_whitespace = false);

}  // namespace flare

#endif  // FLARE_BASE_ENCODING_PERCENT_H_
