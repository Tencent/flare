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

#include "flare/rpc/protocol/protobuf/compression.h"

#include <string>

#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/base/object_pool.h"

namespace flare::protobuf::compression {

TEST(Compress, All) {
  auto meta = object_pool::Get<rpc::RpcMeta>();
  meta->set_compression_algorithm(rpc::COMPRESSION_ALGORITHM_LZ4_FRAME);

  auto parsed = std::make_unique<ProtoMessage>();

  NoncontiguousBufferBuilder body;
  std::string body_str = "hello world";
  body.Append(body_str.data(), body_str.size());
  ProtoMessage msg(std::move(meta), body.DestructiveGet());

  NoncontiguousBufferBuilder compressed;
  EXPECT_GT(CompressBodyIfNeeded(*msg.meta, msg, &compressed), 0);

  NoncontiguousBuffer result;
  EXPECT_TRUE(
      DecompressBodyIfNeeded(*msg.meta, compressed.DestructiveGet(), &result));
  EXPECT_EQ(FlattenSlow(result), body_str);
}

}  // namespace flare::protobuf::compression
