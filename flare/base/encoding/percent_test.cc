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

#include "flare/base/encoding/percent.h"

#include "gtest/gtest.h"

#include "flare/base/string.h"

namespace flare {

TEST(PercentEncoding, Emca262) {
  // Shamelessly copied from:
  //
  // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/encodeURIComponent
  std::string_view set1 = ";,/?:@&=+$";   // Reserved Characters
  std::string_view set2 = "-_.!~*'()";    // Unescaped Characters
  std::string_view set3 = "#";            // Number Sign
  std::string_view set4 = "ABC abc 123";  // Alphanumeric Characters + Space

  // Reserved chars are escaped.

  EXPECT_EQ("%3B%2C%2F%3F%3A%40%26%3D%2B%24",
            EncodePercent(set1, PercentEncodingOptions{
                                    .style = PercentEncodingStyle::Ecma262}));
  EXPECT_EQ("-_.!~*'()",
            EncodePercent(set2, PercentEncodingOptions{
                                    .style = PercentEncodingStyle::Ecma262}));
  EXPECT_EQ("%23",
            EncodePercent(set3, PercentEncodingOptions{
                                    .style = PercentEncodingStyle::Ecma262}));
  EXPECT_EQ("ABC%20abc%20123",
            EncodePercent(set4, PercentEncodingOptions{
                                    .style = PercentEncodingStyle::Ecma262}));

  EXPECT_EQ(set1, DecodePercent("%3B%2C%2F%3F%3A%40%26%3D%2B%24"));
  EXPECT_EQ(set2, DecodePercent("-_.!~*'()"));
  EXPECT_EQ(set3, DecodePercent("%23"));
  EXPECT_EQ(set4, DecodePercent("ABC%20abc%20123"));

  // Reserved chars are kept.
  EXPECT_EQ(";,/?:@&=+$",
            EncodePercent(set1, PercentEncodingOptions{
                                    .style = PercentEncodingStyle::Ecma262,
                                    .escape_reserved = false}));
  EXPECT_EQ("-_.!~*'()",
            EncodePercent(set2, PercentEncodingOptions{
                                    .style = PercentEncodingStyle::Ecma262,
                                    .escape_reserved = false}));
  EXPECT_EQ("#", EncodePercent(set3, PercentEncodingOptions{
                                         .style = PercentEncodingStyle::Ecma262,
                                         .escape_reserved = false}));
  EXPECT_EQ("ABC%20abc%20123",
            EncodePercent(set4, PercentEncodingOptions{
                                    .style = PercentEncodingStyle::Ecma262,
                                    .escape_reserved = false}));

  EXPECT_EQ(set1, DecodePercent(";,/?:@&=+$"));
  EXPECT_EQ(set2, DecodePercent("-_.!~*'()"));
  EXPECT_EQ(set3, DecodePercent("#"));
  EXPECT_EQ(set4, DecodePercent("ABC%20abc%20123"));
}

TEST(PercentEncoding, Rfc3986) {
  std::string_view str = "_-,;:!?.'()[]@*/&#+=~$ABC abc 123";

  EXPECT_EQ(
      "_-%2C%3B%3A%21%3F.%27%28%29%5B%5D%40%2A%2F%26%23%2B%3D~%24ABC%20abc%"
      "20123",
      EncodePercent(
          str, PercentEncodingOptions{.style = PercentEncodingStyle::Rfc3986}));
  EXPECT_EQ("_-,;:!?.'()[]@*/&#+=~$ABC%20abc%20123",
            EncodePercent(str, PercentEncodingOptions{
                                   .style = PercentEncodingStyle::Rfc3986,
                                   .escape_reserved = false}));
  EXPECT_EQ(str,
            DecodePercent("_-%2C%3B%3A%21%3F.%27%28%29%5B%5D%40%2A%2F%26%23%"
                          "2B%3D~%24ABC%20abc%20123"));
  EXPECT_EQ(str, DecodePercent("_-,;:!?.'()[]@*/&#+=~$ABC%20abc%20123"));
}

TEST(PercentEncoding, Rfc5987) {
  // `encodeRFC5987ValueChars from MDN does not seems quite right (in that it
  // does escape `#` `$` ..., which is not required.):
  //
  // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/encodeURIComponent
  //
  // auto encode_rfc5987_value_chars = [](const std::string_view& str) {
  //   auto result =
  //       EncodePercent(str, PercentEncodingOptions{.style =
  //       PercentEncodingStyle::Ecma262});
  //   result = Replace(result, "'", "%" + EncodeHex("'"));
  //   result = Replace(result, "(", "%" + EncodeHex("("));
  //   result = Replace(result, ")", "%" + EncodeHex(")"));
  //   result = Replace(result, "*", "%" + EncodeHex("*"));
  //   result = Replace(result, "%7C", "|");
  //   result = Replace(result, "%60", "`");
  //   result = Replace(result, "%5E", "^");
  //   return result;
  // };
  //
  // EXPECT_EQ(
  //     encode_rfc5987_value_chars(str),
  //     EncodePercent(str, PercentEncodingOptions{.style =
  //     PercentEncodingStyle::Rfc5987}));

  std::string_view str = "!123'-!#$&()*,./:;?@[]^_`|~+=ABC abc";

  EXPECT_EQ("!123%27-!#$&%28%29%2A%2C.%2F%3A%3B%3F%40%5B%5D^_`|~+%3DABC%20abc",
            EncodePercent(str, PercentEncodingOptions{
                                   .style = PercentEncodingStyle::Rfc5987}));
  EXPECT_EQ(
      str,
      DecodePercent(
          "!123%27-!#$&%28%29%2A%2C.%2F%3A%3B%3F%40%5B%5D^_`|~+%3DABC%20abc"));
}

TEST(PercentEncoding, DecodePlusSignAsWhitespace) {
  EXPECT_EQ("a+b", DecodePercent("a+b"));
  EXPECT_EQ("a b", DecodePercent("a+b", true));
}

}  // namespace flare
