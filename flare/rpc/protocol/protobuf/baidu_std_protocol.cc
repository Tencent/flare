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

#include "flare/rpc/protocol/protobuf/baidu_std_protocol.h"

#include <string>

#include "flare/base/buffer/zero_copy_stream.h"
#include "flare/base/down_cast.h"
#include "flare/base/endian.h"
#include "flare/rpc/protocol/protobuf/baidu_std_rpc_meta.pb.h"
#include "flare/rpc/protocol/protobuf/call_context.h"
#include "flare/rpc/protocol/protobuf/call_context_factory.h"
#include "flare/rpc/protocol/protobuf/compression.h"
#include "flare/rpc/protocol/protobuf/message.h"
#include "flare/rpc/protocol/protobuf/rpc_meta.pb.h"
#include "flare/rpc/protocol/protobuf/service_method_locator.h"

namespace flare::protobuf {

// Underscore is NOT allowed in URI "scheme" part, so we use `baidu-std` instead
// of `baidu_std` here.
FLARE_RPC_REGISTER_CLIENT_SIDE_STREAM_PROTOCOL_ARG("baidu-std",
                                                   BaiduStdProtocol, false);
FLARE_RPC_REGISTER_SERVER_SIDE_STREAM_PROTOCOL_ARG("baidu-std",
                                                   BaiduStdProtocol, true);

namespace {

// All members are network byte-order.
struct Header {
  char magic[4];            // "PRPC".
  std::uint32_t body_size;  // Not including `Header`'s size.
  std::uint32_t meta_size;
};

constexpr auto kHeaderSize = sizeof(Header);

static_assert(kHeaderSize == 12);

struct OnWireMessage : public Message {
  OnWireMessage() { SetRuntimeTypeTo<OnWireMessage>(); }
  std::uint64_t GetCorrelationId() const noexcept override {
    return meta.correlation_id();
  }
  Type GetType() const noexcept override { return Type::Single; }

  brpc::RpcMeta meta;
  NoncontiguousBuffer body;
  NoncontiguousBuffer attach;
};

StreamProtocol::Characteristics characteristics = {.name = "BaiduStd"};

brpc::CompressType GetCompressType(rpc::RpcMeta* meta) {
  if (!meta->has_compression_algorithm()) {
    return brpc::COMPRESS_TYPE_NO_COMPRESSION;
  }
  auto compression = meta->compression_algorithm();
  switch (compression) {
    case rpc::COMPRESSION_ALGORITHM_NONE:
      return brpc::COMPRESS_TYPE_NO_COMPRESSION;
    case rpc::COMPRESSION_ALGORITHM_GZIP:
      return brpc::COMPRESS_TYPE_GZIP;
    case rpc::COMPRESSION_ALGORITHM_SNAPPY:
      return brpc::COMPRESS_TYPE_SNAPPY;
    default:
      // Baidu std protocol doesn't support this compression
      // we should clear meta's compression.
      meta->clear_compression_algorithm();
      FLARE_LOG_WARNING_EVERY_SECOND(
          "Baidu std protocol does not support compression {}", compression);
      return brpc::COMPRESS_TYPE_NO_COMPRESSION;
  }
}

bool SetCompressionAlgorithm(rpc::RpcMeta* meta,
                             google::protobuf::uint32 compress_type) {
  auto brpc_compress_type = static_cast<brpc::CompressType>(compress_type);
  switch (brpc_compress_type) {
    case brpc::COMPRESS_TYPE_NO_COMPRESSION:
      return true;
    case brpc::COMPRESS_TYPE_SNAPPY:
      meta->set_compression_algorithm(rpc::COMPRESSION_ALGORITHM_SNAPPY);
      return true;
    case brpc::COMPRESS_TYPE_GZIP:
      meta->set_compression_algorithm(rpc::COMPRESSION_ALGORITHM_GZIP);
      return true;
    default:
      FLARE_LOG_WARNING_EVERY_SECOND("baidu std protocol support only 0-2!");
      return false;
  }
}

}  // namespace

const StreamProtocol::Characteristics& BaiduStdProtocol::GetCharacteristics()
    const {
  return characteristics;
}

const MessageFactory* BaiduStdProtocol::GetMessageFactory() const {
  return &error_message_factory;
}

const ControllerFactory* BaiduStdProtocol::GetControllerFactory() const {
  return &passive_call_context_factory;
}

BaiduStdProtocol::MessageCutStatus BaiduStdProtocol::TryCutMessage(
    NoncontiguousBuffer* buffer, std::unique_ptr<Message>* message) {
  if (buffer->ByteSize() < kHeaderSize) {
    return MessageCutStatus::NotIdentified;
  }

  // Extract the header (and convert the endianness if necessary) first.
  Header hdr;
  FlattenToSlow(*buffer, &hdr, kHeaderSize);
  FromBigEndian(&hdr.body_size);
  FromBigEndian(&hdr.meta_size);

  if (memcmp(hdr.magic, "PRPC", 4) != 0) {
    return MessageCutStatus::ProtocolMismatch;
  }
  if (buffer->ByteSize() < kHeaderSize + hdr.body_size) {
    return MessageCutStatus::NeedMore;
  }
  if (hdr.meta_size > hdr.body_size) {  // Sanity check.
    FLARE_LOG_WARNING_EVERY_SECOND("Invalid header received, dropped.");
    return MessageCutStatus::Error;
  }

  auto cut = buffer->Cut(kHeaderSize + hdr.body_size);
  cut.Skip(kHeaderSize);

  // Parse the meta.
  brpc::RpcMeta meta;
  {
    auto meta_bytes = cut.Cut(hdr.meta_size);
    NoncontiguousBufferInputStream nbis(&meta_bytes);
    if (!meta.ParseFromZeroCopyStream(&nbis)) {
      FLARE_LOG_WARNING_EVERY_SECOND("Invalid meta received, dropped.");
      return MessageCutStatus::Error;
    }
  }

  if (meta.attachment_size() < 0 ||
      static_cast<std::uint64_t>(meta.attachment_size()) + hdr.meta_size >
          hdr.body_size) {  // Sanity check.
    FLARE_LOG_WARNING_EVERY_SECOND("Invalid header received, dropped.");
    return MessageCutStatus::Error;
  }

  // Cut message off.
  auto body_buffer =
      cut.Cut(hdr.body_size - hdr.meta_size - meta.attachment_size());
  auto attach_buffer = cut.Cut(meta.attachment_size());
  FLARE_CHECK(cut.Empty());

  // We've cut the message then.
  auto msg = std::make_unique<OnWireMessage>();
  msg->meta = std::move(meta);
  msg->body = std::move(body_buffer);
  msg->attach = std::move(attach_buffer);
  *message = std::move(msg);
  return MessageCutStatus::Cut;
}

bool BaiduStdProtocol::TryParse(std::unique_ptr<Message>* message,
                                Controller* controller) {
  auto on_wire = cast<OnWireMessage>(message->get());
  auto&& brpc_meta = on_wire->meta;
  auto meta = object_pool::Get<rpc::RpcMeta>();
  MaybeOwning<google::protobuf::Message> unpack_to;
  bool accept_msg_in_bytes;

  if ((server_side_ && !brpc_meta.has_request()) ||
      (!server_side_ && !brpc_meta.has_response())) {
    FLARE_LOG_WARNING_EVERY_SECOND(
        "Corrupted message: Request or response meta is not present. "
        "Correlation ID {}.",
        brpc_meta.correlation_id());
    return false;
  }

  meta->set_correlation_id(brpc_meta.correlation_id());
  meta->set_method_type(rpc::METHOD_TYPE_SINGLE);

  // Set compression algorithm.
  if (!SetCompressionAlgorithm(meta.Get(), brpc_meta.compress_type())) {
    return false;
  }

  if (server_side_) {
    auto&& req_meta = *meta->mutable_request_meta();
    req_meta.set_method_name(brpc_meta.request().service_name() + "." +
                             brpc_meta.request().method_name());
    if (brpc_meta.request().has_log_id()) {
      req_meta.set_request_id(brpc_meta.request().log_id());
    }
    auto&& method = meta->request_meta().method_name();
    auto desc = ServiceMethodLocator::Instance()->TryGetMethodDesc(
        protocol_ids::standard, method);
    if (!desc) {
      FLARE_LOG_WARNING_EVERY_SECOND("Method [{}] is not found.", method);
      *message = std::make_unique<EarlyErrorMessage>(
          brpc_meta.correlation_id(), rpc::STATUS_METHOD_NOT_FOUND,
          fmt::format("Method [{}] is not implemented.", method));
      return true;
    }

    static constexpr std::uint64_t acceptable_compression_algorithms =
        1 << rpc::COMPRESSION_ALGORITHM_NONE |
        1 << rpc::COMPRESSION_ALGORITHM_GZIP |
        1 << rpc::COMPRESSION_ALGORITHM_SNAPPY;
    req_meta.set_acceptable_compression_algorithms(
        acceptable_compression_algorithms);

    unpack_to = std::unique_ptr<google::protobuf::Message>(
        desc->request_prototype->New());
    accept_msg_in_bytes = false;  // TODO(luobogao): Implementation.
  } else {
    FLARE_CHECK(brpc_meta.has_response());  // Checked before.
    auto ctx = cast<ProactiveCallContext>(controller);
    accept_msg_in_bytes = ctx->accept_response_in_bytes;
    if (FLARE_LIKELY(!accept_msg_in_bytes)) {
      unpack_to = ctx->GetOrCreateResponse();
    }

    // FIXME: Error code definition does not match between brpc & flare.
    meta->mutable_response_meta()->set_status(
        brpc_meta.response().error_code());
    if (brpc_meta.response().has_error_text()) {
      meta->mutable_response_meta()->set_description(
          brpc_meta.response().error_text());
    }
  }

  auto parsed = std::make_unique<ProtoMessage>();

  parsed->meta = std::move(meta);
  parsed->attachment = std::move(on_wire->attach);
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
          brpc_meta.correlation_id());
      return false;
    }
    parsed->msg_or_buffer = std::move(unpack_to);
  }

  *message = std::move(parsed);
  return true;
}

// Serialize `message` into `buffer`.
void BaiduStdProtocol::WriteMessage(const Message& message,
                                    NoncontiguousBuffer* buffer,
                                    Controller* controller) {
  auto msg = cast<ProtoMessage>(&message);
  auto&& meta = *msg->meta;
  auto&& att = msg->attachment;
  NoncontiguousBufferBuilder nbb;
  auto reserved_hdr = nbb.Reserve(sizeof(Header));
  Header hdr = {.magic = {'P', 'R', 'P', 'C'}, .body_size = 0, .meta_size = 0};

  // baidu-std does support distributed tracing, but not in a format compatible
  // with us..
  FLARE_LOG_ERROR_IF_ONCE(
      !controller->GetTracingContext().empty() ||
          controller->IsTraceForciblySampled(),
      "Passing tracing context is not supported by BaiduStd protocol.");

  {
    NoncontiguousBufferOutputStream nbos(&nbb);

    // Translate & serialize rpc meta.
    brpc::RpcMeta brpc_meta;
    brpc_meta.set_correlation_id(meta.correlation_id());
    if (att.ByteSize()) {
      brpc_meta.set_attachment_size(att.ByteSize());
    }
    brpc_meta.set_compress_type(GetCompressType(&meta));
    if (server_side_) {
      auto&& bresp_meta = *brpc_meta.mutable_response();
      auto&& resp_meta = meta.response_meta();
      // FIXME: Translate error code.
      bresp_meta.set_error_code(resp_meta.status());
      if (resp_meta.has_description()) {
        bresp_meta.set_error_text(resp_meta.description());
      }
    } else {
      auto&& breq_meta = *brpc_meta.mutable_request();
      auto&& req_meta = meta.request_meta();
      auto last_dot = req_meta.method_name().find_last_of('.');
      FLARE_CHECK(last_dot != std::string::npos, "Unexpected method name [{}]",
                  req_meta.method_name());
      breq_meta.set_service_name(req_meta.method_name().substr(0, last_dot));
      breq_meta.set_method_name(req_meta.method_name().substr(last_dot + 1));
    }
    hdr.body_size = hdr.meta_size = brpc_meta.ByteSizeLong();
    FLARE_CHECK(brpc_meta.SerializeToZeroCopyStream(&nbos));
  }

  hdr.body_size += compression::CompressBodyIfNeeded(meta, *msg, &nbb);
  if (!att.Empty()) {
    nbb.Append(att);  // Attachment.
  }

  // Fill the header.
  ToBigEndian(&hdr.body_size);
  ToBigEndian(&hdr.meta_size);
  memcpy(reserved_hdr, &hdr, sizeof(hdr));

  buffer->Append(nbb.DestructiveGet());
}

}  // namespace flare::protobuf
