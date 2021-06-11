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

#include "flare/rpc/protocol/protobuf/binlog.h"

#include <variant>

#include "googletest/gtest/gtest.h"

#include "flare/rpc/protocol/protobuf/message.h"
#include "flare/testing/message.pb.h"

namespace flare::protobuf {

TEST(Binlog, Normal) {
  ProtoMessage message;

  message.meta = object_pool::Get<rpc::RpcMeta>();
  message.msg_or_buffer = CreateBufferSlow("my buffer");
  message.attachment = CreateBufferSlow("attach");

  auto desc = WritePacketDesc(message);
  EXPECT_EQ(desc.meta, message.meta.Get());
  EXPECT_EQ("my buffer", FlattenSlow(desc.WriteMessage()));
  EXPECT_EQ("attach", FlattenSlow(*desc.attachment));

  message.msg_or_buffer = std::monostate{};
  EXPECT_EQ("", FlattenSlow(WritePacketDesc(message).WriteMessage()));

  testing::One one;
  one.set_str("str");
  message.msg_or_buffer = MaybeOwning(non_owning, &one);
  EXPECT_EQ(one.SerializeAsString(),
            FlattenSlow(WritePacketDesc(message).WriteMessage()));
}

}  // namespace flare::protobuf
