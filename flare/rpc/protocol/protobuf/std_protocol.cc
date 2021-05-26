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

#include "flare/rpc/protocol/protobuf/std_protocol.h"

#include "flare/base/buffer/zero_copy_stream.h"
#include "flare/base/down_cast.h"
#include "flare/base/endian.h"
#include "flare/rpc/protocol/protobuf/call_context.h"
#include "flare/rpc/protocol/protobuf/call_context_factory.h"
#include "flare/rpc/protocol/protobuf/compression.h"
#include "flare/rpc/protocol/protobuf/message.h"
#include "flare/rpc/protocol/protobuf/rpc_meta.pb.h"
#include "flare/rpc/protocol/protobuf/service_method_locator.h"

namespace flare::protobuf {

FLARE_RPC_REGISTER_CLIENT_SIDE_STREAM_PROTOCOL_ARG("flare", StdProtocol, false);
FLARE_RPC_REGISTER_SERVER_SIDE_STREAM_PROTOCOL_ARG("flare", StdProtocol, true);

namespace {

void RegisterMethodCallback(const google::protobuf::MethodDescriptor* method) {
  ServiceMethodLocator::Instance()->RegisterMethod(protocol_ids::standard,
                                                   method, method->full_name());
}

void DeregisterMethodCallback(
    const google::protobuf::MethodDescriptor* method) {
  ServiceMethodLocator::Instance()->DeregisterMethod(protocol_ids::standard,
                                                     method);
}

}  // namespace

FLARE_RPC_PROTOCOL_PROTOBUF_REGISTER_METHOD_PROVIDER(RegisterMethodCallback,
                                                     DeregisterMethodCallback);

namespace {

// All members are little-endian (after serialization).
struct Header {
  std::uint32_t magic;
  std::uint32_t meta_size;
  std::uint32_t msg_size;
  std::uint32_t att_size;
};

constexpr auto kHeaderSize = sizeof(Header);
const std::uint32_t kHeaderMagic = 'F' << 24 | 'R' << 16 | 'P' << 8 | 'C';

static_assert(kHeaderSize == 16);

struct OnWireMessage : public Message {
  OnWireMessage() { SetRuntimeTypeTo<OnWireMessage>(); }
  std::uint64_t GetCorrelationId() const noexcept override {
    return meta->correlation_id();
  }
  Type GetType() const noexcept override {
    return FromWireType(meta->method_type(), meta->flags());
  }

  PooledPtr<rpc::RpcMeta> meta;
  NoncontiguousBuffer body;
  NoncontiguousBuffer attach;
};

StreamProtocol::Characteristics characteristics = {.name = "FlareStd"};

}  // namespace

const StreamProtocol::Characteristics& StdProtocol::GetCharacteristics() const {
  return characteristics;
}

const MessageFactory* StdProtocol::GetMessageFactory() const {
  return &error_message_factory;
}

const ControllerFactory* StdProtocol::GetControllerFactory() const {
  return &passive_call_context_factory;
}

StdProtocol::MessageCutStatus StdProtocol::TryCutMessage(
    NoncontiguousBuffer* buffer, std::unique_ptr<Message>* message) {
  if (buffer->ByteSize() < kHeaderSize) {
    return MessageCutStatus::NotIdentified;
  }

  // Extract the header (and convert the endianness if necessary) first.
  Header hdr;
  FlattenToSlow(*buffer, &hdr, kHeaderSize);
  FromLittleEndian(&hdr.magic);
  FromLittleEndian(&hdr.meta_size);
  FromLittleEndian(&hdr.msg_size);
  FromLittleEndian(&hdr.att_size);

  if (hdr.magic != kHeaderMagic) {
    return MessageCutStatus::ProtocolMismatch;
  }
  if (buffer->ByteSize() < static_cast<std::uint64_t>(kHeaderSize) +
                               hdr.meta_size + hdr.msg_size + hdr.att_size) {
    return MessageCutStatus::NeedMore;
  }

  // Do basic parse.
  auto cut =
      buffer->Cut(kHeaderSize + hdr.meta_size + hdr.msg_size + hdr.att_size);
  cut.Skip(kHeaderSize);

  // Parse the meta.
  auto meta = object_pool::Get<rpc::RpcMeta>();
  bool parsed = false;
  // Since meta is relatively small, there's a chance that it's contiguous
  // physically. In this case we could parse it inline, which should be faster.
  if (cut.FirstContiguous().size() >= hdr.meta_size) {
    parsed = meta->ParseFromArray(cut.FirstContiguous().data(), hdr.meta_size);
    cut.Skip(hdr.meta_size);
  } else {
    auto meta_buffer = cut.Cut(hdr.meta_size);
    NoncontiguousBufferInputStream nbis(&meta_buffer);
    parsed = meta->ParseFromZeroCopyStream(&nbis) && meta_buffer.Empty();
  }

  // We need to consume the body / attachment anyway otherwise we would leave
  // the buffer in non-packet-boundary.
  auto body_buffer = cut.Cut(hdr.msg_size);
  auto attach_buffer = cut.Cut(hdr.att_size);
  FLARE_CHECK(cut.Empty());

  // If parsing meta failed, raise an error now.
  if (!parsed) {
    FLARE_LOG_WARNING_EVERY_SECOND("Invalid meta received, dropped.");
    return MessageCutStatus::Error;
  }

  // We've cut the message then.
  auto msg = std::make_unique<OnWireMessage>();
  msg->meta = std::move(meta);
  msg->body = std::move(body_buffer);
  msg->attach = std::move(attach_buffer);
  *message = std::move(msg);
  return MessageCutStatus::Cut;
}

bool StdProtocol::TryParse(std::unique_ptr<Message>* message,
                           Controller* controller) {
  auto on_wire = cast<OnWireMessage>(message->get());
  auto parsed = std::make_unique<ProtoMessage>();

  // Let's move them early.
  parsed->meta = std::move(on_wire->meta);

  auto&& meta = parsed->meta;

  if ((server_side_ && !meta->has_request_meta()) ||
      (!server_side_ && !meta->has_response_meta())) {
    FLARE_LOG_WARNING_EVERY_SECOND(
        "Corrupted message: Request or response meta is not present. "
        "Correlation ID {}.",
        meta->correlation_id());
    return false;
  }

  // Set only if we really need to deserialize the message.
  MaybeOwning<google::protobuf::Message> unpack_to;

  if (server_side_) {
    auto&& method = meta->request_meta().method_name();
    auto desc = ServiceMethodLocator::Instance()->TryGetMethodDesc(
        protocol_ids::standard, method);
    if (!desc) {
      // Instead of dropping the packet, we could produce an `EarlyErrorMessage`
      // of `kMethodNotFound` and let the upper layer return an error more
      // gracefully.
      FLARE_VLOG(1, "Method [{}] is not found.", method);
      *message = std::make_unique<EarlyErrorMessage>(
          meta->correlation_id(), rpc::STATUS_METHOD_NOT_FOUND,
          fmt::format("Method [{}] is not implemented.", method));
      // TODO(luobogao): We could change `TryParse`'s signature and returns
      // `kNotFound`, and let the framework itself to create the error response.
      return true;
    }
    *controller->MutableTracingContext() =
        meta->request_meta().tracing_context();

    // TODO(luobogao): Implement option `accept_request_raw_bytes`.
    unpack_to = std::unique_ptr<google::protobuf::Message>(
        desc->request_prototype->New());
  } else {
    FLARE_CHECK(meta->has_response_meta());  // Checked before.
    controller->SetTraceForciblySampled(
        meta->response_meta().trace_forcibly_sampled());
    if (auto ctx = cast<ProactiveCallContext>(controller);
        !ctx->accept_response_in_bytes) {
      unpack_to = ctx->GetOrCreateResponse();
    }
  }

  if (FLARE_LIKELY(!(meta->flags() & rpc::MESSAGE_FLAGS_NO_PAYLOAD))) {
    if (FLARE_LIKELY(unpack_to)) {
      NoncontiguousBuffer buffer;
      if (!compression::DecompressBodyIfNeeded(*meta, std::move(on_wire->body),
                                               &buffer)) {
        FLARE_LOG_WARNING_EVERY_SECOND(
            "Failed to decompress message (correlation id {}).",
            meta->correlation_id());
        return false;
      }
      NoncontiguousBufferInputStream nbis(&buffer);
      if (!unpack_to->ParseFromZeroCopyStream(&nbis)) {
        FLARE_LOG_WARNING_EVERY_SECOND(
            "Failed to parse message (correlation id {}).",
            meta->correlation_id());
        return false;
      }
      parsed->msg_or_buffer = std::move(unpack_to);
    } else {
      parsed->msg_or_buffer = std::move(on_wire->body);
    }
  }

  if (!on_wire->attach.Empty()) {
    if (meta->attachment_compressed()) {
      NoncontiguousBuffer buffer;
      if (!compression::DecompressBodyIfNeeded(
              *meta, std::move(on_wire->attach), &buffer)) {
        FLARE_LOG_WARNING_EVERY_SECOND(
            "Failed to decompress message (correlation id {}).",
            meta->correlation_id());
        return false;
      }
      parsed->attachment = std::move(buffer);
    } else {
      parsed->attachment = std::move(on_wire->attach);
    }
  }
  *message = std::move(parsed);
  return true;
}

// Serialize `message` into `buffer`.
void StdProtocol::WriteMessage(const Message& message,
                               NoncontiguousBuffer* buffer,
                               Controller* controller) {
  auto old_size = buffer->ByteSize();
  auto msg = cast<ProtoMessage>(&message);
  auto meta = *msg->meta;  // Copied, likely to be slow.
  auto&& att = msg->attachment;

  if (server_side_) {
    if (controller->IsTraceForciblySampled()) {
      meta.mutable_response_meta()->set_trace_forcibly_sampled(
          controller->IsTraceForciblySampled());
    }
  } else {
    if (!controller->GetTracingContext().empty()) {
      meta.mutable_request_meta()->set_tracing_context(
          controller->GetTracingContext());
    }
  }

  NoncontiguousBufferBuilder nbb;
  auto reserved_for_hdr = nbb.Reserve(sizeof(Header));

  Header hdr = {
      .magic = kHeaderMagic,
      // TODO(luobogao): Serialize first and use `GetCachedSize()` instead.
      .meta_size = static_cast<std::uint32_t>(meta.ByteSizeLong()),
      .msg_size = 0 /* Filled later. */,
      .att_size = 0 /* Filled later. */};

  {
    NoncontiguousBufferOutputStream nbos(&nbb);
    FLARE_CHECK(meta.SerializeToZeroCopyStream(&nbos));  // Meta.
  }

  // Body
  hdr.msg_size += compression::CompressBodyIfNeeded(meta, *msg, &nbb);

  // Attachment.
  if (!att.Empty()) {
    if (meta.attachment_compressed() && !msg->precompressed_attachment) {
      hdr.att_size += compression::CompressBufferIfNeeded(meta, att, &nbb);
    } else {
      nbb.Append(att);
      hdr.att_size += att.ByteSize();
    }
  }

  // Fill header.
  ToLittleEndian(&hdr.magic);
  ToLittleEndian(&hdr.meta_size);
  ToLittleEndian(&hdr.msg_size);
  ToLittleEndian(&hdr.att_size);
  memcpy(reserved_for_hdr, &hdr, sizeof(Header));

  buffer->Append(nbb.DestructiveGet());
  FLARE_CHECK_EQ(buffer->ByteSize() - old_size,
                 kHeaderSize + hdr.meta_size + hdr.msg_size + hdr.att_size);
}

}  // namespace flare::protobuf
