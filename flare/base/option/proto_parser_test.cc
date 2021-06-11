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

#include "flare/base/option/proto_parser.h"

#include "googletest/gtest/gtest.h"

#include "flare/testing/message.pb.h"

namespace flare::option {

TEST(ProtoTextFormatParser, Ok) {
  auto parsed = ProtoTextFormatParser<flare::testing::One>().TryParse(
      R"(str:"str",integer:1)");
  ASSERT_TRUE(parsed);
  EXPECT_EQ("str", parsed->str());
  EXPECT_EQ(1, parsed->integer());
}

TEST(ProtoTextFormatParser, Error) {
  EXPECT_FALSE(ProtoTextFormatParser<flare::testing::One>().TryParse("1234"));
}

}  // namespace flare::option
