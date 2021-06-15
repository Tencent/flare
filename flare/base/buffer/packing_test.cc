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

#include "flare/base/buffer/packing.h"

#include "gtest/gtest.h"

namespace flare {

TEST(Packing, Empty) {
  auto buffer = WriteKeyedNoncontiguousBuffers({});
  EXPECT_FALSE(buffer.Empty());
  auto parsed = TryParseKeyedNoncontiguousBuffers(buffer);
  ASSERT_TRUE(parsed);
  ASSERT_EQ(0, parsed->size());
}

TEST(Packing, KeyedBuffer) {
  auto buffer =
      WriteKeyedNoncontiguousBuffers({{"key1", CreateBufferSlow("value1")},
                                      {"key2", CreateBufferSlow("value2")}});
  EXPECT_FALSE(buffer.Empty());
  auto parsed = TryParseKeyedNoncontiguousBuffers(buffer);
  ASSERT_TRUE(parsed);
  ASSERT_EQ(2, parsed->size());
  EXPECT_EQ("key1", (*parsed)[0].first);
  EXPECT_EQ("value1", FlattenSlow((*parsed)[0].second));
  EXPECT_EQ("key2", (*parsed)[1].first);
  EXPECT_EQ("value2", FlattenSlow((*parsed)[1].second));
}

TEST(Packing, BufferArray) {
  auto buffer = WriteNoncontiguousBufferArray(
      {CreateBufferSlow("value1"), CreateBufferSlow("value2")});
  EXPECT_FALSE(buffer.Empty());
  auto parsed = TryParseNoncontiguousBufferArray(buffer);
  ASSERT_TRUE(parsed);
  ASSERT_EQ(2, parsed->size());
  EXPECT_EQ("value1", FlattenSlow((*parsed)[0]));
  EXPECT_EQ("value2", FlattenSlow((*parsed)[1]));
}

}  // namespace flare
