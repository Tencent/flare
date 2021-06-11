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

#include "flare/base/experimental/uuid.h"

#include "googletest/gtest/gtest.h"

#include "flare/base/string.h"

namespace flare::experimental {

TEST(Uuid, Compare) {
  // Constexpr-friendly.
  static constexpr Uuid kUuid1("123e4567-e89b-12d3-a456-426614174000");
  static constexpr Uuid kUuid2;

  EXPECT_EQ("123e4567-e89b-12d3-a456-426614174000",
            Uuid("123e4567-e89b-12d3-a456-426614174000").ToString());
  EXPECT_EQ("123e4567-e89b-12d3-a456-426614174000", kUuid1.ToString());
  EXPECT_EQ("123e4567-e89b-12d3-a456-426614174000",
            Uuid("123E4567-E89B-12D3-a456-426614174000").ToString());
  EXPECT_EQ("00000000-0000-0000-0000-000000000000", Uuid().ToString());
  EXPECT_EQ("00000000-0000-0000-0000-000000000000", kUuid2.ToString());
}

TEST(Uuid, TryParse) {
  auto parsed = TryParse<Uuid>("123e4567-e89b-12d3-a456-426614174000");
  ASSERT_TRUE(parsed);
  EXPECT_EQ("123e4567-e89b-12d3-a456-426614174000", parsed->ToString());
  EXPECT_FALSE(TryParse<Uuid>("123e4567-e89b-12d3-a456-42661417400"));
  EXPECT_FALSE(TryParse<Uuid>("123e4567-e89b-12d3-a456-4266141740000"));
  EXPECT_FALSE(TryParse<Uuid>("123e4567-e89b-12d3-a456=426614174000"));
  EXPECT_FALSE(TryParse<Uuid>("123e4567-e89b-12d3-a456-42661417400G"));
}

}  // namespace flare::experimental
