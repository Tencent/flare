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

#include "flare/base/buffer/zero_copy_stream.h"

#include <string>
#include <utility>

#include "googletest/gtest/gtest.h"
#include "google/protobuf/util/message_differencer.h"

#include "flare/testing/message.pb.h"

namespace flare {

TEST(ZeroCopyStream, SerializeAndDeserialize) {
  testing::ComplexMessage src;
  src.set_integer(556);
  src.set_enumeration(testing::ENUM_0);
  src.set_str("short str");
  src.mutable_one()->set_str("a missing string" + std::string(16777216, 'a'));

  NoncontiguousBufferBuilder nbb;
  NoncontiguousBufferOutputStream zbos(&nbb);
  CHECK(src.SerializeToZeroCopyStream(&zbos));
  zbos.Flush();
  auto serialized = nbb.DestructiveGet();
  NoncontiguousBuffer cp1 = CreateBufferSlow(FlattenSlow(serialized));
  NoncontiguousBuffer cp2 = serialized;

  // Same size as serialized to string.
  ASSERT_EQ(src.SerializeAsString().size(), serialized.ByteSize());

  NoncontiguousBuffer splited;
  splited.Append(serialized.Cut(1));
  splited.Append(serialized.Cut(2));
  splited.Append(serialized.Cut(3));
  splited.Append(serialized.Cut(4));
  splited.Append(std::move(serialized));

  for (auto&& e : {&cp1, &cp2, &splited}) {
    // Parseable.
    NoncontiguousBufferInputStream nbis(e);
    testing::ComplexMessage dest;
    ASSERT_TRUE(dest.ParseFromZeroCopyStream(&nbis));

    // Same as the original one.
    ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(src, dest));
  }
}

}  // namespace flare
