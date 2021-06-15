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

#include "flare/base/buffer.h"

#include <string>

#include "gtest/gtest.h"

#include "flare/init/override_flag.h"
#include "flare/testing/main.h"

using namespace std::literals;

FLARE_OVERRIDE_FLAG(flare_buffer_block_size, BUFFER_BLOCK_SIZE);

namespace flare {

namespace {

PolymorphicBuffer MakeNativeBuffer(std::string_view s) {
  auto buffer = MakeNativeBufferBlock();
  memcpy(buffer->mutable_data(), s.data(), s.size());
  return PolymorphicBuffer(buffer, 0, s.size());
}

}  // namespace

TEST(CreateBufferSlow, All) {
  static const auto kData = "sadfas234sadf-/+8sdaf sd f~!#"s;
  auto nb = CreateBufferSlow(kData);
  ASSERT_EQ(kData, nb.FirstContiguous().data());
  ASSERT_EQ(kData, FlattenSlow(nb));
}

TEST(NoncontiguousBuffer, Cut) {
  NoncontiguousBuffer nb;
  nb.Append(CreateBufferSlow("asdf"));
  auto r = nb.Cut(3);
  ASSERT_EQ(1, nb.ByteSize());
  ASSERT_EQ("f", FlattenSlow(nb));
  ASSERT_EQ(3, r.ByteSize());
  ASSERT_EQ("asd", FlattenSlow(r));
}

TEST(NoncontiguousBuffer, Cut1) {
  NoncontiguousBuffer nb;
  nb.Append(CreateBufferSlow("asdf"));
  auto r = nb.Cut(4);
  ASSERT_TRUE(nb.Empty());
  ASSERT_EQ(4, r.ByteSize());
}

TEST(NoncontiguousBuffer, Cut2) {
  NoncontiguousBuffer nb;
  nb.Append(MakeNativeBuffer("asdf"));
  nb.Append(MakeNativeBuffer("asdf"));
  auto r = nb.Cut(4);
  ASSERT_EQ(4, nb.ByteSize());
  ASSERT_EQ(4, r.ByteSize());
}

TEST(NoncontiguousBuffer, Cut3) {
  NoncontiguousBuffer nb;
  nb.Append(MakeNativeBuffer("asdf"));
  nb.Append(MakeNativeBuffer("asdf"));
  auto r = nb.Cut(8);
  ASSERT_TRUE(nb.Empty());
  ASSERT_EQ(8, r.ByteSize());
}

TEST(NoncontiguousBuffer, Cut4) {
  auto nb = CreateBufferSlow("asdfasf2345sfsdfdf");
  auto nb2 = nb;
  ASSERT_EQ(FlattenSlow(nb), FlattenSlow(nb2));
  NoncontiguousBuffer splited;
  splited.Append(nb.Cut(1));
  splited.Append(nb.Cut(2));
  splited.Append(nb.Cut(3));
  splited.Append(nb.Cut(4));
  splited.Append(std::move(nb));
  ASSERT_EQ(FlattenSlow(nb2), FlattenSlow(splited));
}

TEST(NoncontiguousBuffer, Skip) {
  NoncontiguousBuffer splited;
  splited.Append(CreateBufferSlow("asdf"));
  splited.Append(CreateBufferSlow("asdf"));
  splited.Append(CreateBufferSlow("asdf"));
  splited.Append(CreateBufferSlow("asdf"));
  splited.Append(CreateBufferSlow("asdf"));
  splited.Append(CreateBufferSlow("asdf"));
  splited.Append(CreateBufferSlow("asdf"));
  splited.Append(CreateBufferSlow("asdf"));
  splited.Skip(32);
  ASSERT_EQ(0, splited.ByteSize());
}

TEST(NoncontiguousBuffer, Skip2) {
  NoncontiguousBuffer buffer;
  EXPECT_TRUE(buffer.Empty());
  buffer.Skip(0);  // Don't crash.
  EXPECT_TRUE(buffer.Empty());
}

TEST(NoncontiguousBuffer, FlattenSlow) {
  NoncontiguousBuffer nb;
  nb.Append(MakeNativeBuffer("asd4234"));
  nb.Append(MakeNativeBuffer("aXsdfsadfasdf2342"));
  ASSERT_EQ("asd4234aXs", FlattenSlow(nb, 10));
}

TEST(NoncontiguousBuffer, FlattenToSlow) {
  struct C {
    std::uint64_t ll;
    int i;
    bool f;
  };
  NoncontiguousBuffer nb;
  nb.Append(MakeNativeBuffer("\x12\x34\x56\x78\x9a\xbc\xde\xf0"s));
  nb.Append(MakeNativeBuffer("\x12\x34\x56\x78"s));
  nb.Append(MakeNativeBuffer("\x1"s));
  nb.Append(MakeNativeBuffer("\x00\x00\x00"s));  // Padding
  C c;
  FlattenToSlow(nb, &c, sizeof(C));
  ASSERT_EQ(0xf0debc9a78563412, c.ll);  // TODO(luobogao): Endianness.
  ASSERT_EQ(0x78563412, c.i);
  ASSERT_EQ(true, c.f);
}

TEST(NoncontiguousBuffer, FlattenSlowUntil) {
  NoncontiguousBuffer nb;
  nb.Append(MakeNativeBuffer("asd4234"));
  nb.Append(MakeNativeBuffer("aXsdfsadfasdf2342"));
  ASSERT_EQ("asd4234aX", FlattenSlowUntil(nb, "aX"));
  ASSERT_EQ("asd4", FlattenSlowUntil(nb, "4"));
  ASSERT_EQ("asd4234aXsdfsadfasdf2342", FlattenSlowUntil(nb, "2342"));
  ASSERT_EQ("asd42", FlattenSlowUntil(nb, "z", 5));
  ASSERT_EQ("asd42", FlattenSlowUntil(nb, "3", 5));
  ASSERT_EQ("asd42", FlattenSlowUntil(nb, "2", 5));
  ASSERT_EQ("asd4", FlattenSlowUntil(nb, "4", 5));
}

TEST(NoncontiguousBuffer, FlattenSlowUntil2) {
  auto nb = CreateBufferSlow(
      "HTTP/1.1 200 OK\r\nRpc-SeqNo: 14563016719\r\nRpc-Error-Code: "
      "0\r\nRpc-Error-Reason: The operation completed "
      "successfully.\r\nContent-Type: "
      "application/x-protobuf\r\nContent-Length: 0\r\n\r\nHTTP/1.1 200 "
      "OK\r\nRpc-Seq");
  ASSERT_EQ(
      "HTTP/1.1 200 OK\r\nRpc-SeqNo: 14563016719\r\nRpc-Error-Code: "
      "0\r\nRpc-Error-Reason: The operation completed "
      "successfully.\r\nContent-Type: "
      "application/x-protobuf\r\nContent-Length: 0\r\n\r\n",
      FlattenSlowUntil(nb, "\r\n\r\n"));
}

TEST(NoncontiguousBuffer, FlattenSlowUntil3) {
  NoncontiguousBuffer nb;
  nb.Append(MakeNativeBuffer("asd4234"));
  nb.Append(MakeNativeBuffer("aXsdfsadfasdf2342"));
  ASSERT_EQ("asd4234aX", FlattenSlowUntil(nb, "aX"));
  ASSERT_EQ("asd4", FlattenSlowUntil(nb, "4"));
  ASSERT_EQ("asd4234aX", FlattenSlowUntil(nb, "4aX"));
}

TEST(NoncontiguousBuffer, FlattenSlowUntil4) {
  NoncontiguousBuffer nb;
  nb.Append(MakeNativeBuffer("AB"));
  nb.Append(MakeNativeBuffer("CDEFGGGGHHHH"));
  ASSERT_EQ("ABCDEFGGGG", FlattenSlowUntil(nb, "GGGG"));
}

TEST(NoncontiguousBufferBuilder, Append) {
  NoncontiguousBufferBuilder nbb;
  nbb.Append(MakeForeignBuffer(""));
  nbb.Append(MakeForeignBuffer("small"));
  nbb.Append(MakeForeignBuffer(std::string(8192, 'a')));
  nbb.Append(CreateBufferSlow(""));
  nbb.Append(CreateBufferSlow("small"));
  nbb.Append(CreateBufferSlow(std::string(8192, 'a')));
  EXPECT_EQ("small" + std::string(8192, 'a') + "small" + std::string(8192, 'a'),
            FlattenSlow(nbb.DestructiveGet()));
}

TEST(NoncontiguousBufferBuilder, Reserve) {
  auto temp_block = MakeNativeBufferBlock();
  auto max_bytes = temp_block->size();
  NoncontiguousBufferBuilder nbb;
  auto ptr = nbb.data();
  auto ptr2 = nbb.Reserve(10);
  ASSERT_EQ(ptr, ptr2);
  ASSERT_EQ(ptr + 10, nbb.data());
  nbb.Append(std::string(max_bytes - 10 - 1, 'a'));
  ptr = nbb.data();
  ptr2 = nbb.Reserve(1);  // Last byte in the block.
  ASSERT_EQ(ptr, ptr2);
  ASSERT_EQ(max_bytes, nbb.SizeAvailable());

  nbb.Append(std::string(max_bytes - 1, 'a'));
  ptr = nbb.data();
  ptr2 = nbb.Reserve(2);
  ASSERT_NE(ptr, ptr2);
  ASSERT_EQ(ptr2 + 2, nbb.data());
}

TEST(NoncontiguousBufferBuilder, DestructiveGet1) {
  NoncontiguousBufferBuilder nbb;
  nbb.Append("asdf1234", 6);
  nbb.Append("1122", 4);
  ASSERT_EQ(
      "asdf12"
      "1122",
      FlattenSlow(nbb.DestructiveGet()));
}

TEST(NoncontiguousBufferBuilder, DestructiveGet2) {
  NoncontiguousBufferBuilder nbb;
  nbb.Append("aabbccd");
  ASSERT_EQ("aabbccd", FlattenSlow(nbb.DestructiveGet()));
}

TEST(NoncontiguousBufferBuilder, DestructiveGet3) {
  NoncontiguousBufferBuilder nbb;
  nbb.Append(std::string(1000000, 'A'));
  ASSERT_EQ(std::string(1000000, 'A'), FlattenSlow(nbb.DestructiveGet()));
}

TEST(NoncontiguousBufferBuilder, DestructiveGet4) {
  NoncontiguousBufferBuilder nbb;
  nbb.Append('c');
  ASSERT_EQ("c", FlattenSlow(nbb.DestructiveGet()));
}

TEST(NoncontiguousBufferBuilder, DestructiveGet5) {
  NoncontiguousBufferBuilder nbb;
  nbb.Append(CreateBufferSlow("c"));
  ASSERT_EQ("c", FlattenSlow(nbb.DestructiveGet()));
}

TEST(NoncontiguousBufferBuilder, DestructiveGet6) {
  NoncontiguousBufferBuilder nbb;
  nbb.Append("11"sv, "2"sv, "3"s, "45"s, "6"sv);
  nbb.Append("1122", 4);
  ASSERT_EQ("11234561122", FlattenSlow(nbb.DestructiveGet()));
}

TEST(MakeReferencingBuffer, Simple) {
  NoncontiguousBufferBuilder nbb;
  nbb.Append(MakeReferencingBuffer("abcdefg", 7));
  EXPECT_EQ("abcdefg", FlattenSlow(nbb.DestructiveGet()));
}

TEST(MakeReferencingBuffer, WithCallbackSmallBufferOptimized) {
  int x = 0;

  NoncontiguousBufferBuilder nbb;
  nbb.Append("aaa", 3);
  // Small buffers are copied by `Append` and freed immediately.
  nbb.Append(MakeReferencingBuffer("abcdefg", 7, [&] { ++x; }));
  // Therefore the callback should have fired on return of `Append`.
  EXPECT_EQ(1, x);
  auto buffer = nbb.DestructiveGet();
  EXPECT_EQ("aaaabcdefg", FlattenSlow(buffer));
}

TEST(MakeReferencingBuffer, WithCallback) {
  static const std::string kBuffer(12345, 'a');
  int x = 0;

  {
    NoncontiguousBufferBuilder nbb;
    nbb.Append("aaa", 3);
    nbb.Append(MakeReferencingBuffer(kBuffer.data(), 1024, [&] { ++x; }));
    EXPECT_EQ(0, x);
    auto buffer = nbb.DestructiveGet();
    EXPECT_EQ(0, x);
    EXPECT_EQ("aaa" + kBuffer.substr(0, 1024), FlattenSlow(buffer));
  }
  EXPECT_EQ(1, x);
}

TEST(MakeForeignBuffer, String) {
  NoncontiguousBufferBuilder nbb;
  nbb.Append(MakeForeignBuffer("abcdefg"s));
  EXPECT_EQ("abcdefg", FlattenSlow(nbb.DestructiveGet()));
}

TEST(MakeForeignBuffer, VectorOfChar) {
  std::vector<char> data{'a', 'b', 'c', 'd', 'e', 'f', 'g'};
  NoncontiguousBufferBuilder nbb;
  nbb.Append(MakeForeignBuffer(std::move(data)));
  EXPECT_EQ("abcdefg", FlattenSlow(nbb.DestructiveGet()));
}

TEST(MakeForeignBuffer, VectorOfBytes) {
  std::vector<std::byte> data;
  data.resize(7);
  memcpy(data.data(), "abcdefg", 7);
  NoncontiguousBufferBuilder nbb;
  nbb.Append(MakeForeignBuffer(std::move(data)));
  EXPECT_EQ("abcdefg", FlattenSlow(nbb.DestructiveGet()));
}

TEST(MakeForeignBuffer, VectorOfUInt8) {
  std::vector<std::uint8_t> data;
  data.resize(7);
  memcpy(data.data(), "abcdefg", 7);
  NoncontiguousBufferBuilder nbb;
  nbb.Append(MakeForeignBuffer(std::move(data)));
  EXPECT_EQ("abcdefg", FlattenSlow(nbb.DestructiveGet()));
}

}  // namespace flare

FLARE_TEST_MAIN
