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

#include "flare/base/crypto/md5.h"

#include "gtest/gtest.h"

#include "flare/base/buffer.h"
#include "flare/base/encoding.h"

using namespace std::literals;

namespace flare {

TEST(Md5, All) {
  auto result = "827ccb0eea8a706c4c34a16891f84e7b";
  EXPECT_EQ(result, EncodeHex(Md5(CreateBufferSlow("12345"))));
  EXPECT_EQ(result, EncodeHex(Md5("12345")));
  EXPECT_EQ(result, EncodeHex(Md5({"123", "45"})));
}

TEST(HmacMd5, All) {
  auto result = "8f8afda40668a73d8dcbee1508559c91";
  EXPECT_EQ(result, EncodeHex(HmacMd5("key", CreateBufferSlow("12345"))));
  EXPECT_EQ(result, EncodeHex(HmacMd5("key", "12345")));
  EXPECT_EQ(result, EncodeHex(HmacMd5("key", {"123", "45"})));
}

}  // namespace flare
