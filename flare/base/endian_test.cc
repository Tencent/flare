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

#include "flare/base/endian.h"

#include <arpa/inet.h>

#include "googletest/gtest/gtest.h"

namespace flare {

constexpr std::uint8_t k8 = 0x12;
constexpr std::uint16_t k16 = 0x1234;
constexpr std::uint32_t k32 = 0x12345678;
constexpr std::uint64_t k64 = 0x1234567890abcdefULL;

// `htnos` in CentOS 7 does not get along well with `ASSERT_XX` ("error:
// statement-expressions are not allowed outside functions nor in
// template-argument lists"), so we define it as a function here.
std::uint16_t Htons(std::uint16_t v) { return htons(v); }
std::uint32_t Htonl(std::uint32_t v) { return htonl(v); }

TEST(Endian, Big) {
  if constexpr (std::endian::native == std::endian::big) {
    ASSERT_EQ(k8, ToBigEndian<std::uint8_t>(k8));
    ASSERT_EQ(k16, ToBigEndian<std::uint16_t>(k16));
    ASSERT_EQ(k32, ToBigEndian<std::uint32_t>(k32));
    ASSERT_EQ(k64, ToBigEndian<std::uint64_t>(k64));
    ASSERT_EQ(k8, FromBigEndian<std::uint8_t>(k8));
    ASSERT_EQ(k16, FromBigEndian<std::uint16_t>(k16));
    ASSERT_EQ(k32, FromBigEndian<std::uint32_t>(k32));
    ASSERT_EQ(k64, FromBigEndian<std::uint64_t>(k64));
  } else {
    ASSERT_EQ(k8, ToBigEndian<std::uint8_t>(k8));
    ASSERT_EQ(Htons(k16), ToBigEndian<std::uint16_t>(k16));
    ASSERT_EQ(Htonl(k32), ToBigEndian<std::uint32_t>(k32));
    ASSERT_EQ(endian::detail::SwapEndian(k64),
              ToBigEndian<std::uint64_t>(k64));  // ...
    ASSERT_EQ(k8, FromBigEndian<std::uint8_t>(k8));
    ASSERT_EQ(Htons(k16), FromBigEndian<std::uint16_t>(k16));
    ASSERT_EQ(Htonl(k32), FromBigEndian<std::uint32_t>(k32));
    ASSERT_EQ(endian::detail::SwapEndian(k64),
              FromBigEndian<std::uint64_t>(k64));  // ...
  }
}

TEST(Endian, Little) {
  if constexpr (std::endian::native == std::endian::little) {
    ASSERT_EQ(k8, ToLittleEndian<std::uint8_t>(k8));
    ASSERT_EQ(k16, ToLittleEndian<std::uint16_t>(k16));
    ASSERT_EQ(k32, ToLittleEndian<std::uint32_t>(k32));
    ASSERT_EQ(k64, ToLittleEndian<std::uint64_t>(k64));
    ASSERT_EQ(k8, FromLittleEndian<std::uint8_t>(k8));
    ASSERT_EQ(k16, FromLittleEndian<std::uint16_t>(k16));
    ASSERT_EQ(k32, FromLittleEndian<std::uint32_t>(k32));
    ASSERT_EQ(k64, FromLittleEndian<std::uint64_t>(k64));
  } else {
    ASSERT_EQ(k8, ToLittleEndian<std::uint8_t>(k8));
    ASSERT_EQ(endian::detail::SwapEndian(k16),
              ToLittleEndian<std::uint16_t>(k16));  // ...
    ASSERT_EQ(endian::detail::SwapEndian(k32),
              ToLittleEndian<std::uint32_t>(k32));
    ASSERT_EQ(endian::detail::SwapEndian(k64),
              ToLittleEndian<std::uint64_t>(k64));
    ASSERT_EQ(k8, FromLittleEndian<std::uint8_t>(k8));
    ASSERT_EQ(endian::detail::SwapEndian(k16),
              FromLittleEndian<std::uint16_t>(k16));
    ASSERT_EQ(endian::detail::SwapEndian(k32),
              FromLittleEndian<std::uint32_t>(k32));
    ASSERT_EQ(endian::detail::SwapEndian(k64),
              FromLittleEndian<std::uint64_t>(k64));
  }
}

}  // namespace flare
