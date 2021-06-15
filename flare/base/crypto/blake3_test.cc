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

#include "flare/base/crypto/blake3.h"

#include "gtest/gtest.h"

#include "flare/base/buffer.h"
#include "flare/base/encoding.h"

using namespace std::literals;

namespace flare {

TEST(Blake3, All) {
  EXPECT_EQ("a6f2f4910b1c9d582d32cdc76a0fe844d9d98082099d13b65f69b20d66fd517b",
            EncodeHex(Blake3(CreateBufferSlow("hello."))));
  EXPECT_EQ("a6f2f4910b1c9d582d32cdc76a0fe844d9d98082099d13b65f69b20d66fd517b",
            EncodeHex(Blake3("hello.")));
  EXPECT_EQ("a6f2f4910b1c9d582d32cdc76a0fe844d9d98082099d13b65f69b20d66fd517b",
            EncodeHex(Blake3({"hel", "lo."})));
}

TEST(Blake3Digest, All) {
  Blake3Digest digest;
  digest.Append("h", 1);
  digest.Append("e");
  digest.Append({"ll", "o", "."});
  EXPECT_EQ("a6f2f4910b1c9d582d32cdc76a0fe844d9d98082099d13b65f69b20d66fd517b",
            EncodeHex(digest.DestructiveGet()));
}

}  // namespace flare
