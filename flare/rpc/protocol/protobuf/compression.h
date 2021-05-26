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

#ifndef FLARE_RPC_PROTOCOL_PROTOBUF_COMPRESSION_H_
#define FLARE_RPC_PROTOCOL_PROTOBUF_COMPRESSION_H_

#include "flare/base/buffer.h"
#include "flare/rpc/protocol/protobuf/message.h"
#include "flare/rpc/protocol/protobuf/rpc_meta.pb.h"

namespace flare::protobuf::compression {

bool DecompressBodyIfNeeded(const rpc::RpcMeta& meta, NoncontiguousBuffer body,
                            NoncontiguousBuffer* buffer);

std::size_t CompressBodyIfNeeded(const rpc::RpcMeta& meta,
                                 const ProtoMessage& msg,
                                 NoncontiguousBufferBuilder* builder);

std::size_t CompressBufferIfNeeded(const rpc::RpcMeta& meta,
                                   const NoncontiguousBuffer& buffer,
                                   NoncontiguousBufferBuilder* builder);

}  // namespace flare::protobuf::compression

#endif  // FLARE_RPC_PROTOCOL_PROTOBUF_COMPRESSION_H_
