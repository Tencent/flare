// Copyright (C) 2011 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "flare/base/experimental/byte_set.h"

#include "gtest/gtest.h"

namespace flare::experimental {

constexpr ByteSet empty;
constexpr ByteSet digits("0123456789");
constexpr ByteSet uppers("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
constexpr ByteSet lowers("abcdefghijklmnopqrstuvwxyz");
constexpr ByteSet alphas(uppers | lowers);
constexpr ByteSet alnums(alphas | digits);

TEST(ByteSet, Empty) {
  for (int i = 0; i < UCHAR_MAX; ++i) {
    EXPECT_FALSE(empty.contains(i));
  }
}

TEST(ByteSet, InsertAndFind) {
  ByteSet bs;
  EXPECT_FALSE(bs.contains('A'));
  bs.insert('A');
  EXPECT_TRUE(bs.contains('A'));
  for (int i = 0; i < UCHAR_MAX; ++i) {
    EXPECT_EQ(i >= 'A' && i <= 'Z', uppers.contains(i));
    EXPECT_EQ(i >= '0' && i <= '9', digits.contains(i));
  }
}

TEST(ByteSet, CharPtr) {
  const char* s = "ABCD";
  ByteSet bs(s);
  const char* const cs = "ABCD";
  ByteSet cbs(cs);
  EXPECT_EQ(bs, cbs);
}

TEST(ByteSet, Or) {
  EXPECT_EQ(alphas, uppers | lowers);
  EXPECT_EQ(alnums, alphas | digits);
}

TEST(ByteSet, And) {
  EXPECT_EQ(empty, uppers & lowers);
  EXPECT_EQ(alnums, alphas | digits);
}

TEST(ByteSet, OrEq) {
  ByteSet bs(lowers);
  bs |= uppers;
  EXPECT_EQ(alphas, bs);
}

TEST(ByteSet, AndEq) {
  ByteSet bs(alnums);
  bs &= digits;
  EXPECT_EQ(digits, bs);
}

}  // namespace flare::experimental
