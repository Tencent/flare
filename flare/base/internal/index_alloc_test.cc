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

#include "flare/base/internal/index_alloc.h"

#include "gtest/gtest.h"

namespace flare::internal {

struct Tag1;
struct Tag2;

TEST(IndexAlloc, All) {
  auto&& ia1 = IndexAlloc::For<Tag1>();
  auto&& ia2 = IndexAlloc::For<Tag2>();
  ASSERT_EQ(0, ia1->Next());
  ASSERT_EQ(1, ia1->Next());
  ASSERT_EQ(0, ia2->Next());
  ASSERT_EQ(2, ia1->Next());
  ia1->Free(1);
  ASSERT_EQ(1, ia2->Next());
  ASSERT_EQ(1, ia1->Next());
  ASSERT_EQ(2, ia2->Next());
}

}  // namespace flare::internal
