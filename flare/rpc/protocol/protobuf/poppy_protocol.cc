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

#include "flare/rpc/protocol/protobuf/poppy_protocol.h"

#include <string>

#include "flare/base/buffer/zero_copy_stream.h"
#include "flare/base/down_cast.h"
#include "flare/base/endian.h"
#include "flare/base/string.h"
#include "flare/rpc/protocol/protobuf/call_context.h"
#include "flare/rpc/protocol/protobuf/call_context_factory.h"
#include "flare/rpc/protocol/protobuf/compression.h"
#include "flare/rpc/protocol/protobuf/message.h"
#include "flare/rpc/protocol/protobuf/poppy_rpc_meta.pb.h"
#include "flare/rpc/protocol/protobuf/rpc_meta.pb.h"
#include "flare/rpc/protocol/protobuf/service_method_locator.h"

using namespace std::literals;

namespace flare::protobuf {

FLARE_RPC_REGISTER_CLIENT_SIDE_STREAM_PROTOCOL_ARG("poppy", PoppyProtocol,
                                                   false);
FLARE_RPC_REGISTER_SERVER_SIDE_STREAM_PROTOCOL_ARG("poppy", PoppyProtocol,
                                                   true);

namespace {

// All members are network byte-order.
struct Header {
  std::uint32_t meta_size;
  std::uint32_t body_size;
};

constexpr auto kHeaderSize = sizeof(Header);

static_assert(kHeaderSize == 8);

struct OnWireMessage : public Message {
  OnWireMessage() { SetRuntimeTypeTo<OnWireMessage>(); }
  std::uint64_t GetCorrelationId() const noexcept override {
    return meta.sequence_id();
  }
  Type GetType() const noexcept override { return Type::Single; }

  poppy::RpcMeta meta;
  NoncontiguousBuffer body;
};

StreamProtocol::Characteristics characteristics = {.name = "Poppy"};

poppy::CompressType GetCompressType(rpc::RpcMeta* meta) {
  if (!meta->has_compression_algorithm()) {
    return poppy::COMPRESS_TYPE_NONE;
  }
  auto compression = meta->compression_algorithm();
  switch (compression) {
    case rpc::COMPRESSION_ALGORITHM_NONE:
      return poppy::COMPRESS_TYPE_NONE;
    case rpc::COMPRESSION_ALGORITHM_SNAPPY:
      return poppy::COMPRESS_TYPE_SNAPPY;
    default:
      // The compression algorithm specified is not supported by Poppy.
      meta->clear_compression_algorithm();
      FLARE_LOG_WARNING_ONCE(
          "Compression algorithm [{}] is not supported by Poppy. Failing back "
          "to no compression.",
          rpc::CompressionAlgorithm_Name(compression));
      return poppy::COMPRESS_TYPE_NONE;
  }
}

bool SetCompressionAlgorithm(rpc::RpcMeta* meta,
                             google::protobuf::uint32 compress_type) {
  auto poppy_compress = static_cast<poppy::CompressType>(compress_type);
  switch (poppy_compress) {
    case poppy::COMPRESS_TYPE_NONE:
      return true;
    case poppy::COMPRESS_TYPE_SNAPPY:
      meta->set_compression_algorithm(rpc::COMPRESSION_ALGORITHM_SNAPPY);
      return true;
    default:
      FLARE_LOG_WARNING_EVERY_SECOND(
          "Unexpected compression algorithm #{} received.", compress_type);
      return false;
  }
}

}  // namespace

const StreamProtocol::Characteristics& PoppyProtocol::GetCharacteristics()
    const {
  return characteristics;
}

const MessageFactory* PoppyProtocol::GetMessageFactory() const {
  return &error_message_factory;
}

const ControllerFactory* PoppyProtocol::GetControllerFactory() const {
  return &passive_call_context_factory;
}

PoppyProtocol::MessageCutStatus PoppyProtocol::TryCutMessage(
    NoncontiguousBuffer* buffer, std::unique_ptr<Message>* message) {
  if (FLARE_UNLIKELY(!handshake_in_done_)) {
    auto status = KeepHandshakingIn(buffer);
    if (status != MessageCutStatus::Cut) {
      return status;
    }
    // Fall-through otherwise.
  }

  if (buffer->ByteSize() < kHeaderSize) {
    return MessageCutStatus::NeedMore;
  }

  // Extract the header (and convert the endianness if necessary) first.
  Header hdr;
  FlattenToSlow(*buffer, &hdr, kHeaderSize);
  FromBigEndian(&hdr.meta_size);
  FromBigEndian(&hdr.body_size);

  if (buffer->ByteSize() < kHeaderSize + hdr.meta_size + hdr.body_size) {
    return MessageCutStatus::NeedMore;
  }

  // `msg_bytes` is not filled. We're going to remove that argument soon.
  buffer->Skip(kHeaderSize);

  // Parse the meta.
  poppy::RpcMeta meta;
  {
    auto meta_bytes = buffer->Cut(hdr.meta_size);
    NoncontiguousBufferInputStream nbis(&meta_bytes);
    if (!meta.ParseFromZeroCopyStream(&nbis)) {
      FLARE_LOG_WARNING_EVERY_SECOND("Invalid meta received, dropped.");
      return MessageCutStatus::Error;
    }
  }

  // We've cut the message then.
  auto msg = std::make_unique<OnWireMessage>();
  msg->meta = std::move(meta);
  msg->body = buffer->Cut(hdr.body_size);  // Cut message off.
  *message = std::move(msg);
  return MessageCutStatus::Cut;
}

bool PoppyProtocol::TryParse(std::unique_ptr<Message>* message,
                             Controller* controller) {
  auto on_wire = cast<OnWireMessage>(message->get());
  auto&& poppy_meta = on_wire->meta;
  auto meta = object_pool::Get<rpc::RpcMeta>();
  MaybeOwning<google::protobuf::Message> unpack_to;
  bool accept_msg_in_bytes;

  if ((server_side_ && !poppy_meta.has_method()) ||
      (!server_side_ && !poppy_meta.has_failed())) {
    FLARE_LOG_WARNING_EVERY_SECOND(
        "Corrupted message: Essential fields not present. Correlation ID {}.",
        poppy_meta.sequence_id());
    return false;
  }

  meta->set_correlation_id(poppy_meta.sequence_id());
  meta->set_method_type(rpc::METHOD_TYPE_SINGLE);

  // Set compression algorithm.
  if (!SetCompressionAlgorithm(meta.Get(), poppy_meta.compress_type())) {
    return false;
  }

  if (server_side_) {
    auto&& req_meta = *meta->mutable_request_meta();
    req_meta.set_method_name(poppy_meta.method());
    auto&& method = meta->request_meta().method_name();
    auto desc = ServiceMethodLocator::Instance()->TryGetMethodDesc(
        protocol_ids::standard, method);
    if (!desc) {
      FLARE_LOG_WARNING_EVERY_SECOND("Method [{}] is not found.", method);
      *message = std::make_unique<EarlyErrorMessage>(
          poppy_meta.sequence_id(), rpc::STATUS_METHOD_NOT_FOUND,
          fmt::format("Method [{}] is not implemented.", method));
      return true;
    }

    // Not exactly. TODO(luobogao): Use what we've negotiated in handshaking
    // phase.
    static constexpr std::uint64_t kAcceptableCompressionAlgorithms =
        1 << rpc::COMPRESSION_ALGORITHM_NONE |
        1 << rpc::COMPRESSION_ALGORITHM_SNAPPY;
    req_meta.set_acceptable_compression_algorithms(
        kAcceptableCompressionAlgorithms);

    unpack_to = std::unique_ptr<google::protobuf::Message>(
        desc->request_prototype->New());
    accept_msg_in_bytes = false;  // TODO(luobogao): Implementation.
  } else {
    auto ctx = cast<ProactiveCallContext>(controller);
    accept_msg_in_bytes = ctx->accept_response_in_bytes;
    if (FLARE_LIKELY(!accept_msg_in_bytes)) {
      unpack_to = ctx->GetOrCreateResponse();
    }

    if (!poppy_meta.failed()) {
      meta->mutable_response_meta()->set_status(rpc::STATUS_SUCCESS);
    } else {
      // FIXME: Error code definition does not match between brpc & flare.
      meta->mutable_response_meta()->set_status(poppy_meta.error_code());
      if (poppy_meta.has_reason()) {
        meta->mutable_response_meta()->set_description(poppy_meta.reason());
      }
    }
  }

  auto parsed = std::make_unique<ProtoMessage>();

  parsed->meta = std::move(meta);
  if (FLARE_UNLIKELY(accept_msg_in_bytes)) {
    parsed->msg_or_buffer = std::move(on_wire->body);
  } else {
    NoncontiguousBuffer* buffer = &on_wire->body;
    if (!compression::DecompressBodyIfNeeded(*parsed->meta, on_wire->body,
                                             buffer)) {
      FLARE_LOG_WARNING_EVERY_SECOND(
          "Failed to decompress message (correlation id {}).",
          parsed->meta->correlation_id());
      return false;
    }
    NoncontiguousBufferInputStream nbis(buffer);
    if (!unpack_to->ParseFromZeroCopyStream(&nbis)) {
      FLARE_LOG_WARNING_EVERY_SECOND(
          "Failed to parse message (correlation id {}).",
          poppy_meta.sequence_id());
      return false;
    }
    parsed->msg_or_buffer = std::move(unpack_to);
  }

  *message = std::move(parsed);
  return true;
}

// Serialize `message` into `buffer`.
void PoppyProtocol::WriteMessage(const Message& message,
                                 NoncontiguousBuffer* buffer,
                                 Controller* controller) {
  NoncontiguousBufferBuilder nbb;
  if (FLARE_UNLIKELY(!handeshake_out_done_)) {
    KeepHandshakingOut(&nbb);
    // Fall-through otherwise.
  }

  auto msg = cast<ProtoMessage>(&message);
  auto&& meta = *msg->meta;
  auto reserved_hdr = nbb.Reserve(sizeof(Header));

  FLARE_LOG_ERROR_IF_ONCE(!msg->attachment.Empty(),
                          "Attachment is not supported by Poppy protocol");
  FLARE_LOG_ERROR_IF_ONCE(
      !controller->GetTracingContext().empty() ||
          controller->IsTraceForciblySampled(),
      "Passing tracing context is not supported by Poppy protocol.");

  Header hdr;
  {
    NoncontiguousBufferOutputStream nbos(&nbb);

    // Translate & serialize rpc meta.
    poppy::RpcMeta poppy_meta;
    poppy_meta.set_sequence_id(meta.correlation_id());
    poppy_meta.set_compress_type(GetCompressType(&meta));
    if (server_side_) {
      auto&& resp_meta = meta.response_meta();
      poppy_meta.set_failed(resp_meta.status() != rpc::STATUS_SUCCESS);
      poppy_meta.set_error_code(resp_meta.status());  // FIXME: Translate it.
      poppy_meta.set_reason(resp_meta.description());
    } else {
      auto&& req_meta = meta.request_meta();
      poppy_meta.set_method(req_meta.method_name());
      poppy_meta.set_timeout(req_meta.timeout());
    }
    hdr.meta_size = poppy_meta.ByteSizeLong();
    FLARE_CHECK(poppy_meta.SerializeToZeroCopyStream(&nbos));
  }
  hdr.body_size = compression::CompressBodyIfNeeded(meta, *msg, &nbb);

  // Fill the header.
  ToBigEndian(&hdr.body_size);
  ToBigEndian(&hdr.meta_size);
  memcpy(reserved_hdr, &hdr, sizeof(hdr));

  buffer->Append(nbb.DestructiveGet());
}

// Called in single-threaded environment. Each `PoppyProtocol` is bound to
// exactly one connection, thus we can't be called concurrently.
PoppyProtocol::MessageCutStatus PoppyProtocol::KeepHandshakingIn(
    NoncontiguousBuffer* buffer) {
  if (server_side_) {
    static constexpr auto kSignature = "POST /__rpc_service__ HTTP/1.1\r\n"sv;
    if (buffer->ByteSize() < kSignature.size()) {
      return MessageCutStatus::NotIdentified;
    }
    if (FlattenSlow(*buffer, kSignature.size()) != kSignature) {
      return MessageCutStatus::ProtocolMismatch;
    }
  }

  // I'm not sure if we need these headers but let's keep them anyway..
  auto flatten = FlattenSlowUntil(*buffer, "\r\n\r\n");
  if (!EndsWith(flatten, "\r\n\r\n")) {
    return MessageCutStatus::NeedMore;
  }

  buffer->Skip(flatten.size());  // Cut the handshake data off.

  auto splited = Split(flatten, "\r\n");
  splited.erase(splited.begin());  // Skip Start-Line
  for (auto&& e : splited) {
    auto kvs = Split(e, ":", true /* Keep empty */);
    if (kvs.size() != 2) {
      FLARE_LOG_WARNING_EVERY_SECOND(
          "Failed to handshake with the remote side: Unexpected HTTP header "
          "[{}].",
          e);
      return MessageCutStatus::Error;
    }
    conn_headers_[std::string(Trim(kvs[0]))] = std::string(Trim(kvs[1]));
  }

  handshake_in_done_ = true;
  return MessageCutStatus::Cut;
}

// Always called in single-threaded environment.
void PoppyProtocol::KeepHandshakingOut(NoncontiguousBufferBuilder* builder) {
  // Well, hard-code here should work adequately well.
  if (server_side_) {
    builder->Append(
        "HTTP/1.1 200 OK\r\n"
        "X-Poppy-Compress-Type: 0,1\r\n\r\n");  // No-compression & snappy.
  } else {
    builder->Append(
        "POST /__rpc_service__ HTTP/1.1\r\n"
        "Cookie: POPPY_AUTH_TICKET=\r\n"  // Allow channel options to override
                                          // this?
        "X-Poppy-Compress-Type: 0,1\r\n"
        "X-Poppy-Tos: 96\r\n\r\n");  // We don't support TOS.
  }

  handeshake_out_done_ = true;
}

}  // namespace flare::protobuf
