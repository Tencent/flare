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

#include "flare/base/internal/cpu.h"

#include "gmock/gmock-matchers.h"
#include "gtest/gtest.h"

namespace flare::internal {

TEST(TryParseProcesserList, All) {
  EXPECT_FALSE(TryParseProcesserList("-12345678"));
  EXPECT_FALSE(TryParseProcesserList("a-b"));
  EXPECT_FALSE(TryParseProcesserList("1-a"));
  EXPECT_FALSE(TryParseProcesserList("2-1"));
  auto opt = TryParseProcesserList("1-3,4-4,6,-1");
  ASSERT_TRUE(opt);
  EXPECT_THAT(*opt, ::testing::UnorderedElementsAre(
                        1, 2, 3, 4, 6, GetNumberOfProcessorsConfigured() - 1));
}

}  // namespace flare::internal
