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

#include "flare/base/option/json_parser.h"

#include "googletest/gtest/gtest.h"

namespace flare::option {

TEST(JsonParser, Ok) {
  auto parsed = JsonParser().TryParse(R"({"key":"value"})");
  ASSERT_TRUE(parsed);
  EXPECT_EQ("value", (*parsed)["key"].asString());
}

TEST(JsonParser, Error) {
  EXPECT_FALSE(JsonParser().TryParse(R"({"key":"value})"));
}

}  // namespace flare::option
