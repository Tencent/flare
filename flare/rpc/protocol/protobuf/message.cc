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

#include "flare/rpc/protocol/protobuf/message.h"

#include <string>

#include "gflags/gflags.h"

#include "flare/base/buffer/zero_copy_stream.h"
#include "flare/base/enum.h"
#include "flare/base/logging.h"
#include "flare/base/string.h"

DEFINE_int32(
    flare_rpc_protocol_buffers_status_code_for_overloaded,
    flare::rpc::STATUS_OVERLOADED,
    "This flag controls what status code should be returned (to Protocol "
    "Buffers requests) when the server is overloaded. The default is "
    "`STATUS_OVERLOADED`. See `rpc_meta.proto` for more options.");

namespace flare::protobuf {

namespace {

std::unique_ptr<Message> CreateErrorMessage(std::uint64_t correlation_id,
                                            bool stream, int status,
                                            std::string desc) {
  auto meta = object_pool::Get<rpc::RpcMeta>();
  meta->set_correlation_id(correlation_id);
  meta->set_method_type(stream ? rpc::METHOD_TYPE_STREAM
                               : rpc::METHOD_TYPE_SINGLE);
  if (stream) {
    meta->set_flags(rpc::MESSAGE_FLAGS_START_OF_STREAM |
                    rpc::MESSAGE_FLAGS_END_OF_STREAM);  // Both are set.
  }
  auto&& resp_meta = *meta->mutable_response_meta();
  resp_meta.set_status(status);
  resp_meta.set_description(std::move(desc));
  return std::make_unique<ProtoMessage>(std::move(meta), nullptr);
}

}  // namespace

ErrorMessageFactory error_message_factory;

NoncontiguousBuffer Write(const MessageOrBytes& msg) {
  NoncontiguousBufferBuilder nbb;
  WriteTo(msg, &nbb);
  return nbb.DestructiveGet();
}

std::size_t WriteTo(const MessageOrBytes& msg,
                    NoncontiguousBufferBuilder* builder) {
  if (msg.index() == 0) {
    return 0;
  } else if (FLARE_LIKELY(msg.index() == 1)) {
    NoncontiguousBufferOutputStream nbos(builder);
    auto&& pb_msg = std::get<1>(msg);
    if (FLARE_LIKELY(pb_msg)) {
      // `msg->InInitialized()` is not checked here, it's too slow to be checked
      // in optimized build. For non-optimized build, it's already checked by
      // default (by Protocol Buffers' generated code).
      FLARE_CHECK(pb_msg->SerializeToZeroCopyStream(&nbos));
      return pb_msg->GetCachedSize();
    } else {
      return 0;
    }
  } else {
    FLARE_CHECK_EQ(msg.index(), 2);
    auto&& buffer = std::get<2>(msg);
    builder->Append(buffer);
    return buffer.ByteSize();
  }
}

EarlyErrorMessage::EarlyErrorMessage(std::uint64_t correlation_id,
                                     rpc::Status status, std::string desc)
    : correlation_id_(correlation_id), status_(status), desc_(std::move(desc)) {
  FLARE_CHECK(status != rpc::STATUS_SUCCESS);  // Non-intended use.
  SetRuntimeTypeTo<EarlyErrorMessage>();
}

std::uint64_t EarlyErrorMessage::GetCorrelationId() const noexcept {
  return correlation_id_;
}

Message::Type EarlyErrorMessage::GetType() const noexcept {
  return Type::Single;
}

rpc::Status EarlyErrorMessage::GetStatus() const { return status_; }

const std::string& EarlyErrorMessage::GetDescription() const { return desc_; }

std::unique_ptr<Message> ErrorMessageFactory::Create(
    Type type, std::uint64_t correlation_id, bool stream) const {
  if (type == Type::Overloaded || type == Type::CircuitBroken) {
    return CreateErrorMessage(
        correlation_id, stream,
        FLAGS_flare_rpc_protocol_buffers_status_code_for_overloaded,
        "Server overloaded.");
  }
  FLARE_LOG_WARNING_EVERY_SECOND(
      "Unknown message: type {}, correlation_id {}, stream {}.",
      underlying_value(type), correlation_id, stream);
  return nullptr;
}

Message::Type FromWireType(rpc::MethodType method_type, std::uint64_t flags) {
  if (method_type == rpc::METHOD_TYPE_SINGLE) {
    return Message::Type::Single;
  } else if (method_type == rpc::METHOD_TYPE_STREAM) {
    auto rc = Message::Type::Stream;
    if (flags & rpc::MESSAGE_FLAGS_START_OF_STREAM) {
      rc |= Message::Type::StartOfStream;
    }
    if (flags & rpc::MESSAGE_FLAGS_END_OF_STREAM) {
      rc |= Message::Type::EndOfStream;
    }
    return rc;
  }
  FLARE_CHECK(0);
}

}  // namespace flare::protobuf
