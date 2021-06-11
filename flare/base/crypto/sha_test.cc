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

#include "flare/base/crypto/sha.h"

#include "googletest/gtest/gtest.h"

#include "flare/base/buffer.h"
#include "flare/base/encoding.h"

using namespace std::literals;

namespace flare {

TEST(Sha1, All) {
  auto result = "8cb2237d0679ca88db6464eac60da96345513964";
  EXPECT_EQ(result, EncodeHex(Sha1(CreateBufferSlow("12345"))));
  EXPECT_EQ(result, EncodeHex(Sha1("12345")));
  EXPECT_EQ(result, EncodeHex(Sha1({"123", "45"})));
}

TEST(Sha224, All) {
  auto result = "a7470858e79c282bc2f6adfd831b132672dfd1224c1e78cbf5bcd057";
  EXPECT_EQ(result, EncodeHex(Sha224(CreateBufferSlow("12345"))));
  EXPECT_EQ(result, EncodeHex(Sha224("12345")));
  EXPECT_EQ(result, EncodeHex(Sha224({"123", "45"})));
}

TEST(Sha256, All) {
  auto result =
      "5994471abb01112afcc18159f6cc74b4f511b99806da59b3caf5a9c173cacfc5";
  EXPECT_EQ(result, EncodeHex(Sha256(CreateBufferSlow("12345"))));
  EXPECT_EQ(result, EncodeHex(Sha256("12345")));
  EXPECT_EQ(result, EncodeHex(Sha256({"123", "45"})));
}

TEST(Sha384, All) {
  auto result =
      "0fa76955abfa9dafd83facca8343a92aa09497f98101086611b0bfa95dbc0dcc661d62e9"
      "568a5a032ba81960f3e55d4a";
  EXPECT_EQ(result, EncodeHex(Sha384(CreateBufferSlow("12345"))));
  EXPECT_EQ(result, EncodeHex(Sha384("12345")));
  EXPECT_EQ(result, EncodeHex(Sha384({"123", "45"})));
}

TEST(Sha512, All) {
  auto result =
      "3627909a29c31381a071ec27f7c9ca97726182aed29a7ddd2e54353322cfb30abb9e3a6d"
      "f2ac2c20fe23436311d678564d0c8d305930575f60e2d3d048184d79";
  EXPECT_EQ(result, EncodeHex(Sha512(CreateBufferSlow("12345"))));
  EXPECT_EQ(result, EncodeHex(Sha512("12345")));
  EXPECT_EQ(result, EncodeHex(Sha512({"123", "45"})));
}

TEST(HmacSha1, All) {
  auto result = "6cbf4f11135c2fdebe66433f18747d01edc933d1";
  EXPECT_EQ(result, EncodeHex(HmacSha1("key", CreateBufferSlow("12345"))));
  EXPECT_EQ(result, EncodeHex(HmacSha1("key", "12345")));
  EXPECT_EQ(result, EncodeHex(HmacSha1("key", {"123", "45"})));
}

TEST(HmacSha224, All) {
  auto result = "5daf091c83dfa322a6632f0c58b3a7de04e35684443e30b8ee2d0409";
  EXPECT_EQ(result, EncodeHex(HmacSha224("key", CreateBufferSlow("12345"))));
  EXPECT_EQ(result, EncodeHex(HmacSha224("key", "12345")));
  EXPECT_EQ(result, EncodeHex(HmacSha224("key", {"123", "45"})));
}

TEST(HmacSha256, All) {
  auto result =
      "ab99a81f96d56f3b99596e3168b1ade13e02ab0aae08898b8aa4e3377c9e29d1";
  EXPECT_EQ(result, EncodeHex(HmacSha256("key", CreateBufferSlow("12345"))));
  EXPECT_EQ(result, EncodeHex(HmacSha256("key", "12345")));
  EXPECT_EQ(result, EncodeHex(HmacSha256("key", {"123", "45"})));
}

TEST(HmacSha384, All) {
  auto result =
      "1c55c7d4417f36d31a588d23d165b920bf3adc8fae36435c9e1ae490290ead5a5ee4f53d"
      "13df197ab9d231866d5c09a4";
  EXPECT_EQ(result, EncodeHex(HmacSha384("key", CreateBufferSlow("12345"))));
  EXPECT_EQ(result, EncodeHex(HmacSha384("key", "12345")));
  EXPECT_EQ(result, EncodeHex(HmacSha384("key", {"123", "45"})));
}

TEST(HmacSha512, All) {
  auto result =
      "555862d7c9c05c94fab36a2db7c19e055ba0f3300c81188e30c1c7684dd122103d0a640d"
      "ce6b8b6f23e90733dcd262a84aa88e2eb1bc7c7cea21bb346bc2511c";
  EXPECT_EQ(result, EncodeHex(HmacSha512("key", CreateBufferSlow("12345"))));
  EXPECT_EQ(result, EncodeHex(HmacSha512("key", "12345")));
  EXPECT_EQ(result, EncodeHex(HmacSha512("key", {"123", "45"})));
}

}  // namespace flare
