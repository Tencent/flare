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

#include "flare/base/compression.h"

#include "googletest/gtest/gtest.h"

namespace flare {

TEST(MakeCompressor, All) {
  auto&& c = MakeCompressor("gzip");
  EXPECT_TRUE(c);
  c = MakeCompressor("??");
  EXPECT_FALSE(c);
}

TEST(MakeDecompressor, All) {
  auto&& c = MakeDecompressor("gzip");
  EXPECT_TRUE(c);
  c = MakeDecompressor("??");
  EXPECT_FALSE(c);
}

TEST(CompressString, All) {
  std::string original(1000, 'A');
  auto c = Compress(MakeCompressor("gzip").get(), original);
  EXPECT_TRUE(c);
  auto d = Decompress(MakeDecompressor("gzip").get(), *c);
  EXPECT_TRUE(d);
  EXPECT_EQ(FlattenSlow(*d), original);
}

TEST(CompressNoncontiguousBuffer, All) {
  NoncontiguousBufferBuilder nbb;
  std::string original(1000, 'A');
  nbb.Append(original.data(), original.size());
  auto&& nb = nbb.DestructiveGet();
  auto c = Compress(MakeCompressor("gzip").get(), nb);
  EXPECT_TRUE(c);
  auto d = Decompress(MakeDecompressor("gzip").get(), *c);
  EXPECT_TRUE(d);
  EXPECT_EQ(FlattenSlow(*d), original);
}

TEST(Decompressor, Empty) {
  for (auto&& algo : {"zstd", "snappy", "gzip", "lz4-frame"}) {
    auto res = Decompress(MakeDecompressor(algo).get(), "");

    // Failure in decompressing empty buffer is not an error. So long as
    // decompressing a buffer produced by `Compress(..., "")` works correctly,
    // we're fine.
    if (res) {
      // However, if the decompression does succeed, the resulting buffer should
      // be empty.
      EXPECT_TRUE(res->Empty());
    }
  }
}

TEST(Decompressor, Invalid) {
  for (auto&& algo : {"zstd", "snappy", "gzip", "lz4-frame"}) {
    auto res =
        Decompress(MakeDecompressor(algo).get(),
                   "this buffer is likely an invalid compressed buffer.");
    EXPECT_FALSE(res);
  }
}

}  // namespace flare
