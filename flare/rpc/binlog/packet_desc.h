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

#ifndef FLARE_RPC_BINLOG_PACKET_DESC_H_
#define FLARE_RPC_BINLOG_PACKET_DESC_H_

#include <string>
#include <variant>

#include "flare/base/buffer.h"
#include "flare/base/casting.h"
#include "flare/base/experimental/lazy_eval.h"
#include "flare/base/internal/early_init.h"

namespace google::protobuf {

class Message;

}  // namespace google::protobuf

namespace flare {

class NoncontiguousBuffer;
class HttpRequest;
class HttpResponse;

}  // namespace flare

namespace flare::rpc {

class RpcMeta;

}

namespace flare::binlog {

// For clients of backend services to be dry-run aware, they construct an
// instance of subclass of this base class for `Dumper` to inspect on.
//
// Normally subclasses only contains several pointers to real data to be sent /
// received.
class PacketDesc : public ExactMatchCastable {
 public:
  virtual ~PacketDesc() = default;

  // Describes the packet. Note that this method is only used for exposition
  // purpose, and may not fully reflect the packet. Besides, a packet descriptor
  // of low QoI may even return an empty string.
  //
  // Performance wise, the implementation is NOT guaranteed to be efficient. If
  // possible, the dumper is encouraged to inspect the packet by itself.
  virtual experimental::LazyEval<std::string> Describe() const = 0;
};

// Usually packet descriptors inherit from this base class, to simplify
// programming complexity.
template <class T>
struct TypedPacketDesc : PacketDesc {
  TypedPacketDesc() { SetRuntimeTypeTo<T>(); }
};

// Belows are some built-in `PacketDesc`s.
//
// They do not have to be defined here, anywhere accessible to binlog provider
// implementation should work. For the sake of convenience of implementation, we
// define them here.

// Represents a protobuf-based packet.
//
// This type of `PacketDesc` is used by `rpc/protocol/protobuf/`.
//
// TODO(luobogao): Move it into `protobuf/packet_desc.h`.
struct ProtoPacketDesc : TypedPacketDesc<ProtoPacketDesc> {
  const rpc::RpcMeta* meta;

  // Not necessarily, but `instance` can points to actual message. In this case
  // `instance` is not of much use. However, if the payload is provided in terms
  // of `const NoncontiguousBuffer*`, you can use `instance` as a prototype to
  // create a new message object to deserialize the buffer.
  //
  // TODO(luobogao): Not provided yet.
  //
  // const google::protobuf::Message* instance;

  // Points to actual body message.
  //
  // If raw bytes are used, or no payload is provided at all, a (possibly empty)
  // `NoncontiguousBuffer` is provided.
  //
  // We never supply a `nullptr` here.
  std::variant<const google::protobuf::Message*, const NoncontiguousBuffer*>
      message;

  // `attachment` will never be empty. If no attachment is involved, this field
  // points to an empty buffer.
  const NoncontiguousBuffer* attachment =
      &internal::EarlyInitConstant<NoncontiguousBuffer>();

  ProtoPacketDesc() {}

  ProtoPacketDesc(const rpc::RpcMeta& meta,
                  const google::protobuf::Message& message,
                  const NoncontiguousBuffer& attachment)
      : meta(&meta), message(&message), attachment(&attachment) {}

  ProtoPacketDesc(const rpc::RpcMeta& meta, const NoncontiguousBuffer& message,
                  const NoncontiguousBuffer& attachment)
      : meta(&meta), message(&message), attachment(&attachment) {}

  experimental::LazyEval<std::string> Describe() const override;

  // Serialize `message` as buffer. Not necessarily required, provided for the
  // sake of implementation simplicity.
  //
  // Note that this method serializes `message` only (no `meta` / `attachment` /
  // ...).
  NoncontiguousBuffer WriteMessage() const;
};

// More types may be defined in the future (or in other headers). Here is not an
// exhaustive list of descriptors.

}  // namespace flare::binlog

#endif  // FLARE_RPC_BINLOG_PACKET_DESC_H_
