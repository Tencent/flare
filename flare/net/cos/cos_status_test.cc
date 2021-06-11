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

#include "flare/net/cos/cos_status.h"

#include "googletest/gtest/gtest.h"

namespace flare::cos {

TEST(CosStatus, ParseFromString) {
  auto str =
      "<?xml version='1.0' encoding='utf-8' ?>"
      "<Error>"
      "    <Code>ActionAccelerateNotSupported</Code>"
      "    <Message>string</Message>"
      "    <Resource>string</Resource>"
      "    <RequestId>string</RequestId>"
      "    <TraceId>string</TraceId>"
      "</Error>";
  auto parsed = ParseCosStatus(HttpStatus::BadRequest, str);
  EXPECT_EQ(static_cast<int>(CosStatus::ActionAccelerateNotSupported),
            parsed.code());
}

TEST(CosStatus, SpeciallyMapped) {
  auto parsed = ParseCosStatus(HttpStatus::NotModified, "");
  EXPECT_EQ(static_cast<int>(CosStatus::NotModified), parsed.code());
}

}  // namespace flare::cos
