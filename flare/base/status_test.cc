// Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "flare/base/status.h"

#include "googletest/gtest/gtest.h"

namespace flare {

TEST(Status, Success) {
  Status st;
  ASSERT_TRUE(st.ok());
  ASSERT_EQ(0, st.code());
  ASSERT_EQ("", st.message());
  Status st2 = st;
  ASSERT_TRUE(st2.ok());
  ASSERT_EQ(0, st2.code());
  ASSERT_EQ("", st2.message());
}

TEST(Status, Failure) {
  Status st(1, "err");
  ASSERT_FALSE(st.ok());
  ASSERT_EQ(1, st.code());
  ASSERT_EQ("err", st.message());
  Status st2 = st;
  ASSERT_FALSE(st2.ok());
  ASSERT_EQ(1, st2.code());
  ASSERT_EQ("err", st2.message());
}

enum class SomeEnum { Enum1 = 2 };

TEST(Status, FromEnum) {
  Status st(SomeEnum::Enum1);
  EXPECT_EQ(2, st.code());
}

}  // namespace flare
