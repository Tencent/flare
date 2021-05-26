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

#ifndef FLARE_RPC_PROTOCOL_PROTOBUF_BINLOG_H_
#define FLARE_RPC_PROTOCOL_PROTOBUF_BINLOG_H_

#include "flare/rpc/binlog/packet_desc.h"
#include "flare/rpc/protocol/protobuf/message.h"

namespace flare::protobuf {

// Build a packet descriptor that's recognized by binlog subsystem.
//
// The resulting descriptor is invalidated once `msg` is mutated.
binlog::ProtoPacketDesc WritePacketDesc(const ProtoMessage& msg);

}  // namespace flare::protobuf

#endif  // FLARE_RPC_PROTOCOL_PROTOBUF_BINLOG_H_
