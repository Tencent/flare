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

#ifndef FLARE_RPC_PROTOCOL_PROTOBUF_MESSAGE_H_
#define FLARE_RPC_PROTOCOL_PROTOBUF_MESSAGE_H_

#include <limits>
#include <string>
#include <variant>

#include "flare/base/buffer.h"
#include "flare/base/enum.h"
#include "flare/base/maybe_owning.h"
#include "flare/base/object_pool.h"
#include "flare/rpc/protocol/message.h"
#include "flare/rpc/protocol/protobuf/rpc_meta.pb.h"

namespace flare::protobuf {

// Either a:
//
// - Empty state: no payload is present.
// - A pointer to actual message.
// - Binary bytes: `accept_xxx_in_raw_bytes` is applied.
//
// TODO(luobogao): We need a dedicated C++ type for this (for cleaner code.).
using MessageOrBytes =
    std::variant<std::monostate, MaybeOwning<const google::protobuf::Message>,
                 NoncontiguousBuffer>;

// Serializes `MessageOrBytes` to binary bytes.
NoncontiguousBuffer Write(const MessageOrBytes& msg);

// Same as `Write`, but this one writes to an existing buffer builder.
//
// Number of bytes written is returned.
std::size_t WriteTo(const MessageOrBytes& msg,
                    NoncontiguousBufferBuilder* builder);

struct ProtoMessage : Message {
  ProtoMessage() { SetRuntimeTypeTo<ProtoMessage>(); }
  ProtoMessage(PooledPtr<rpc::RpcMeta> meta, MessageOrBytes&& msg_or_buffer,
               NoncontiguousBuffer attachment = {})
      : meta(std::move(meta)),
        msg_or_buffer(std::move(msg_or_buffer)),
        attachment(std::move(attachment)) {
    SetRuntimeTypeTo<ProtoMessage>();
  }

  std::uint64_t GetCorrelationId() const noexcept override {
    return meta->correlation_id();
  }
  Type GetType() const noexcept override;

  PooledPtr<rpc::RpcMeta> meta;
  MessageOrBytes msg_or_buffer;
  NoncontiguousBuffer attachment;

  // Set if `attachment` is already compressed using algorithm specified in
  // `meta`.
  bool precompressed_attachment = false;
};

// Recognized by `StreamService`. Used as a placeholder for notifying
// `StreamService` (and others) that some error occurred during parsing ("early
// stage") the message.
//
// Generating this message (via `ServerProtocol`) differs from returning error
// from protocol / service object in that by doing this, we'll be able to handle
// cases such as "method not found" more gracefully, without abruptly closing
// the connection.
class EarlyErrorMessage : public Message {
 public:
  explicit EarlyErrorMessage(std::uint64_t correlation_id, rpc::Status status,
                             std::string desc);

  std::uint64_t GetCorrelationId() const noexcept override;
  Type GetType() const noexcept override;

  rpc::Status GetStatus() const;
  const std::string& GetDescription() const;

 private:
  std::uint64_t correlation_id_;
  rpc::Status status_;
  std::string desc_;
};

// Factory for creating special messages.
class ErrorMessageFactory : public MessageFactory {
 public:
  std::unique_ptr<Message> Create(Type type, std::uint64_t correlation_id,
                                  bool stream) const override;
};

extern ErrorMessageFactory error_message_factory;

Message::Type FromWireType(rpc::MethodType method_type, std::uint64_t flags);

inline Message::Type ProtoMessage::GetType() const noexcept {
  return FromWireType(meta->method_type(), meta->flags());
}

}  // namespace flare::protobuf

namespace flare {

// AFAICT, using object pool for RpcMeta should performs even better than
// Protocol Buffer's own Arena allocation. The latter cannot eliminate
// allocation of std::string (as of 3.6.1, @sa:
// https://github.com/protocolbuffers/protobuf/issues/4327), while object pool
// can (to some degree).
//
// The reason why object pool helps in this scenario is that Protocol Buffer's
// generated code tries to use existing fields (including messages & strings)
// whenever possible, and when we're parsing a string into an existing message
// who's corresponding fields had ever contained a longer string, the memory
// allocation for the string's internal buffer is eliminated.

template <>
struct PoolTraits<rpc::RpcMeta> {
  static constexpr auto kType = PoolType::MemoryNodeShared;
  static constexpr auto kLowWaterMark = 8192;
  static constexpr auto kHighWaterMark =
      std::numeric_limits<std::size_t>::max();
  static constexpr auto kMaxIdle = std::chrono::seconds(10);
  static constexpr auto kMinimumThreadCacheSize = 1024;
  // 100 transfers per second for 1M QPS.
  static constexpr auto kTransferBatchSize = 1024;

  static void OnGet(rpc::RpcMeta* p) { p->Clear(); }
};

template <>
struct is_enum_bitmask_enabled<rpc::MessageFlags> {
  static constexpr auto value = true;
};

}  // namespace flare

#endif  // FLARE_RPC_PROTOCOL_PROTOBUF_MESSAGE_H_
