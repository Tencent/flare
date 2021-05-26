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

#include "flare/rpc/protocol/protobuf/trpc_protocol.h"

#include <limits>
#include <string>
#include <variant>

#include "thirdparty/protobuf/util/json_util.h"

#include "flare/base/buffer.h"
#include "flare/base/buffer/zero_copy_stream.h"
#include "flare/base/endian.h"
#include "flare/base/string.h"
#include "flare/rpc/protocol/protobuf/call_context.h"
#include "flare/rpc/protocol/protobuf/call_context_factory.h"
#include "flare/rpc/protocol/protobuf/compression.h"
#include "flare/rpc/protocol/protobuf/message.h"
#include "flare/rpc/protocol/protobuf/service_method_locator.h"
#include "flare/rpc/protocol/protobuf/trpc.pb.h"

namespace flare {

template <>
struct PoolTraits<trpc::RequestProtocol> {
  static constexpr auto kType = PoolType::MemoryNodeShared;
  static constexpr auto kLowWaterMark = 8192;
  static constexpr auto kHighWaterMark =
      std::numeric_limits<std::size_t>::max();
  static constexpr auto kMaxIdle = std::chrono::seconds(10);
  static constexpr auto kMinimumThreadCacheSize = 1024;
  static constexpr auto kTransferBatchSize = 1024;
  static void OnGet(trpc::RequestProtocol* p) { p->Clear(); }
};

template <>
struct PoolTraits<trpc::ResponseProtocol> {
  static constexpr auto kType = PoolType::MemoryNodeShared;
  static constexpr auto kLowWaterMark = 8192;
  static constexpr auto kHighWaterMark =
      std::numeric_limits<std::size_t>::max();
  static constexpr auto kMaxIdle = std::chrono::seconds(10);
  static constexpr auto kMinimumThreadCacheSize = 1024;
  static constexpr auto kTransferBatchSize = 1024;
  static void OnGet(trpc::ResponseProtocol* p) { p->Clear(); }
};

}  // namespace flare

namespace flare::protobuf {

FLARE_RPC_REGISTER_CLIENT_SIDE_STREAM_PROTOCOL_ARG("trpc", TrpcProtocol, false);
FLARE_RPC_REGISTER_SERVER_SIDE_STREAM_PROTOCOL_ARG("trpc", TrpcProtocol, true);

namespace {

void RegisterMethodCallback(const google::protobuf::MethodDescriptor* method) {
  ServiceMethodLocator::Instance()->RegisterMethod(
      protocol_ids::trpc, method,
      Format("/{}/{}", method->service()->full_name(), method->name()));
}

void DeregisterMethodCallback(
    const google::protobuf::MethodDescriptor* method) {
  ServiceMethodLocator::Instance()->DeregisterMethod(protocol_ids::trpc,
                                                     method);
}

}  // namespace

FLARE_RPC_PROTOCOL_PROTOBUF_REGISTER_METHOD_PROVIDER(RegisterMethodCallback,
                                                     DeregisterMethodCallback);

namespace {

// UNTESTED. HACK. ONLY WORKS FOR TJG.
//
// In Trpc's case, `tjgtracer::Tracer::Inject(TextMapWriter&)` is used to inject
// tracing context. Internally that method added a KV pair, with key equal to
// constant `tjg::trace::ext::SPANCONTEXT` (evaluates to the same string as
// defined below), and the value being exactly what we've stored in
// `RpcMeta.request_meta.tracing_context`.
//
// @sa: tjg::trace::ext::SPANCONTEXT
constexpr auto kTracingContextKey = "spancontext";

// All numbers are network byte order (i.e., big endian).
struct TrpcHeader {
  std::uint16_t magic;
  std::uint8_t type;  // `DataFrameType`
  std::uint8_t state;
  std::uint32_t total_size;
  std::uint16_t header_size;
  std::uint16_t stream_id;  // Applicable only if `type == TRPC_STREAM_FRAME`.
  char reserved[4];

  void Encode() {
    ToBigEndian(&magic);
    ToBigEndian(&total_size);
    ToBigEndian(&header_size);
    ToBigEndian(&stream_id);
  }

  void Decode() {
    FromBigEndian(&magic);
    FromBigEndian(&total_size);
    FromBigEndian(&header_size);
    FromBigEndian(&stream_id);
  }
};

static_assert(sizeof(TrpcHeader) == 16);

struct OnWireMessage : public Message {
  OnWireMessage() { SetRuntimeTypeTo<OnWireMessage>(); }
  std::uint64_t GetCorrelationId() const noexcept override {
    return request_id;
  }
  Type GetType() const noexcept override { return Type::Single; }

  TrpcHeader header;
  std::uint32_t request_id;
  std::variant<PooledPtr<trpc::RequestProtocol>,
               PooledPtr<trpc::ResponseProtocol>>
      meta;
  NoncontiguousBuffer body;
};

trpc::TrpcCompressType GetCompressType(rpc::RpcMeta* meta) {
  if (!meta->has_compression_algorithm()) {
    return trpc::TRPC_DEFAULT_COMPRESS;
  }
  auto compression = meta->compression_algorithm();
  switch (compression) {
    case rpc::COMPRESSION_ALGORITHM_NONE:
      return trpc::TRPC_DEFAULT_COMPRESS;
    case rpc::COMPRESSION_ALGORITHM_GZIP:
      return trpc::TRPC_GZIP_COMPRESS;
    case rpc::COMPRESSION_ALGORITHM_SNAPPY:
      return trpc::TRPC_SNAPPY_COMPRESS;
    default:
      // Trpc doesn't support this compression
      // we should clear meta's compression.
      meta->clear_compression_algorithm();
      FLARE_LOG_WARNING_EVERY_SECOND("Trpc does not support compression {}",
                                     compression);
      return trpc::TRPC_DEFAULT_COMPRESS;
  }
}

bool SetCompressionAlgorithm(rpc::RpcMeta* meta,
                             google::protobuf::uint32 compress_type) {
  auto trpc_compress_type = static_cast<trpc::TrpcCompressType>(compress_type);
  switch (trpc_compress_type) {
    case trpc::TRPC_DEFAULT_COMPRESS:
      return true;
    case trpc::TRPC_GZIP_COMPRESS:
      meta->set_compression_algorithm(rpc::COMPRESSION_ALGORITHM_GZIP);
      return true;
    case trpc::TRPC_SNAPPY_COMPRESS:
      meta->set_compression_algorithm(rpc::COMPRESSION_ALGORITHM_SNAPPY);
      return true;
    default:
      // Flare doesn't support this compression
      FLARE_LOG_WARNING_EVERY_SECOND("Flare does not support compression {}",
                                     compress_type);
      return false;
  }
}

}  // namespace

const TrpcProtocol::Characteristics& TrpcProtocol::GetCharacteristics() const {
  static const Characteristics characteristics = {.name = "trpc"};
  return characteristics;
}

const MessageFactory* TrpcProtocol::GetMessageFactory() const {
  return &error_message_factory;
}

const ControllerFactory* TrpcProtocol::GetControllerFactory() const {
  return &passive_call_context_factory;
}

TrpcProtocol::MessageCutStatus TrpcProtocol::TryCutMessage(
    NoncontiguousBuffer* buffer, std::unique_ptr<Message>* message) {
  TrpcHeader header;
  if (buffer->ByteSize() < sizeof(header)) {
    return MessageCutStatus::NotIdentified;
  }
  FlattenToSlow(*buffer, &header, sizeof(header));
  header.Decode();
  if (header.magic != trpc::TRPC_MAGIC_VALUE) {
    return MessageCutStatus::ProtocolMismatch;
  }
  if (buffer->ByteSize() < header.total_size) {
    return MessageCutStatus::NeedMore;
  }
  // Basic sanity check.
  if (header.header_size + sizeof(TrpcHeader) > header.total_size) {
    FLARE_LOG_ERROR_EVERY_SECOND("Malformed packet. Dropped.");
    return MessageCutStatus::Error;
  }

  auto on_wire = std::make_unique<OnWireMessage>();
  on_wire->header = header;

  buffer->Skip(sizeof(TrpcHeader));  // We've read it.
  auto meta = buffer->Cut(header.header_size);
  on_wire->body =
      buffer->Cut(header.total_size - header.header_size - sizeof(TrpcHeader));
  if (server_side_) {
    auto req_meta = object_pool::Get<trpc::RequestProtocol>();
    NoncontiguousBufferInputStream nbis(&meta);
    if (!req_meta->ParseFromZeroCopyStream(&nbis)) {
      FLARE_LOG_WARNING_EVERY_SECOND(
          "Failed to parse message header, dropped.");
      return MessageCutStatus::Error;
    }
    on_wire->request_id = req_meta->request_id();
    on_wire->meta = std::move(req_meta);
  } else {
    auto resp_meta = object_pool::Get<trpc::ResponseProtocol>();
    NoncontiguousBufferInputStream nbis(&meta);
    if (!resp_meta->ParseFromZeroCopyStream(&nbis)) {
      FLARE_LOG_WARNING_EVERY_SECOND(
          "Failed to parse message header, dropped.");
      return MessageCutStatus::Error;
    }
    on_wire->request_id = resp_meta->request_id();
    on_wire->meta = std::move(resp_meta);
  }

  *message = std::move(on_wire);
  return MessageCutStatus::Cut;
}

bool TrpcProtocol::TryParse(std::unique_ptr<Message>* message,
                            Controller* controller) {
  auto on_wire = cast<OnWireMessage>(message->get());
  MaybeOwning<google::protobuf::Message> unpack_to;
  bool accept_msg_in_bytes;
  auto rpc_meta = object_pool::Get<rpc::RpcMeta>();
  std::uint32_t content_type = trpc::TRPC_PROTO_ENCODE;

  rpc_meta->set_correlation_id(on_wire->GetCorrelationId());
  rpc_meta->set_method_type(rpc::METHOD_TYPE_SINGLE);

  if (server_side_) {
    auto&& meta = std::get<0>(on_wire->meta);
    auto&& req_meta = rpc_meta->mutable_request_meta();
    // Converts `/trpc.test.helloworld.Greeter/SayHello` to
    // `trpc.test.helloworld.Greeter.SayHello`.
    if (meta->func().empty()) {
      FLARE_LOG_WARNING_EVERY_SECOND(
          "Empty method name is requested by call #{}.",
          on_wire->GetCorrelationId());
      return false;
    }
    auto desc = ServiceMethodLocator::Instance()->TryGetMethodDesc(
        protocol_ids::trpc, meta->func());
    if (!desc) {
      *message = std::make_unique<EarlyErrorMessage>(
          on_wire->GetCorrelationId(), rpc::STATUS_METHOD_NOT_FOUND,
          fmt::format("Method [{}] is not implemented.", meta->func()));
      return true;
    }

    // Set compression algorithm.
    if (!SetCompressionAlgorithm(rpc_meta.Get(), meta->content_encoding())) {
      return false;
    }

    // Translate the meta message.
    req_meta->mutable_method_name()->assign(
        desc->normalized_method_name.begin(),
        desc->normalized_method_name.end());
    // @sa: Comments on `kTracingContextKey`.
    if (auto iter = meta->trans_info().find(kTracingContextKey);
        iter != meta->trans_info().end()) {
      controller->SetTracingContext(std::move(iter->second));
    }

    static constexpr std::uint64_t acceptable_compression_algorithms =
        1 << rpc::COMPRESSION_ALGORITHM_NONE |
        1 << rpc::COMPRESSION_ALGORITHM_GZIP |
        1 << rpc::COMPRESSION_ALGORITHM_SNAPPY;
    req_meta->set_acceptable_compression_algorithms(
        acceptable_compression_algorithms);

    unpack_to = std::unique_ptr<google::protobuf::Message>(
        desc->request_prototype->New());
    content_type = meta->content_type();
    accept_msg_in_bytes = false;  // TODO(luobogao): Allow this.

    // `WriteMessage` needs this.
    cast<PassiveCallContext>(controller)->trpc_content_type = content_type;
  } else {
    auto&& meta = std::get<1>(on_wire->meta);
    auto&& resp_meta = rpc_meta->mutable_response_meta();
    auto ctx = cast<ProactiveCallContext>(controller);

    content_type = meta->content_type();
    accept_msg_in_bytes = ctx->accept_response_in_bytes;
    if (FLARE_LIKELY(!accept_msg_in_bytes)) {
      unpack_to = ctx->GetOrCreateResponse();
    }

    // Set compression algorithm.
    if (!SetCompressionAlgorithm(rpc_meta.Get(), meta->content_encoding())) {
      return false;
    }

    resp_meta->set_status(meta->ret() == trpc::TRPC_INVOKE_SUCCESS
                              ? rpc::STATUS_SUCCESS
                              : meta->func_ret());
    if (!meta->error_msg().empty()) {
      resp_meta->set_description(meta->error_msg());
    }
  }

  auto parsed = std::make_unique<ProtoMessage>();

  parsed->meta = std::move(rpc_meta);
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

    if (content_type == trpc::TRPC_PROTO_ENCODE) {
      NoncontiguousBufferInputStream nbis(buffer);
      if (!unpack_to->ParseFromZeroCopyStream(&nbis)) {
        FLARE_LOG_WARNING_EVERY_SECOND(
            "Failed to parse message (correlation id {}).",
            parsed->meta->correlation_id());
        return false;
      }
    } else if (content_type == trpc::TRPC_JSON_ENCODE) {
      auto status = google::protobuf::util::JsonStringToMessage(
          FlattenSlow(*buffer), unpack_to.Get());
      if (!status.ok()) {
        FLARE_LOG_WARNING_EVERY_SECOND(
            "Failed to parse message (correlation id {}): {}",
            parsed->meta->correlation_id(), status.ToString());
        return false;
      }
    } else {
      FLARE_LOG_WARNING_EVERY_SECOND(
          "Unsupported content type {} (correlation id {}).", content_type,
          parsed->meta->correlation_id());
      return false;
    }

    parsed->msg_or_buffer = std::move(unpack_to);
  }

  *message = std::move(parsed);
  return true;
}

void TrpcProtocol::WriteMessage(const Message& message,
                                NoncontiguousBuffer* buffer,
                                Controller* controller) {
  auto msg = cast<ProtoMessage>(message);
  NoncontiguousBufferBuilder builder;
  auto unaligned_header = builder.Reserve(sizeof(TrpcHeader));  // Filled later.
  TrpcHeader header;
  std::uint32_t content_type = trpc::TRPC_PROTO_ENCODE;

  header.magic = trpc::TRPC_MAGIC_VALUE;
  header.type = trpc::TRPC_UNARY_FRAME;
  header.state = 0;
  header.stream_id = 0;  // Not used.
  // Sizes are filled later.

  auto&& meta = *msg->meta;
  FLARE_CHECK_LE(meta.correlation_id(),
                 std::numeric_limits<std::uint32_t>::max(),
                 "Unexpected: Correlation ID overflow. BUG in the framework?");

  // Serialize header.
  if (server_side_) {
    content_type = cast<PassiveCallContext>(controller)->trpc_content_type;

    // Trpc chooses a rather weird solution for implementing this: They
    // serialize the entire `SpanContext` and propagate it backwards to the
    // caller, only to read a single key (`ext::kTraceExtTraceErrorFlag`) to
    // know if the callee want to report the trace.
    //
    // Given that Trpc protocol is not widely used by our users, let's keep it
    // simple for the moment.
    FLARE_LOG_ERROR_IF_ONCE(controller->IsTraceForciblySampled(),
                            "Backwards propagation of trace sampling decision "
                            "is not supported when using Trpc protocol.");
    trpc::ResponseProtocol resp;
    auto&& resp_meta = meta.response_meta();
    resp.set_version(trpc::TRPC_PROTO_V1);
    resp.set_call_type(trpc::TRPC_UNARY_CALL);
    resp.set_request_id(meta.correlation_id());
    resp.set_ret(resp_meta.status() == rpc::STATUS_SUCCESS
                     ? trpc::TRPC_INVOKE_SUCCESS
                     : trpc::TRPC_SERVER_SYSTEM_ERR);
    resp.set_func_ret(resp_meta.status());
    if (!resp_meta.description().empty()) {
      resp.set_error_msg(resp_meta.description());
    }
    resp.set_content_type(content_type);
    resp.set_content_encoding(GetCompressType(&meta));

    header.header_size = resp.ByteSizeLong();
    NoncontiguousBufferOutputStream nbos(&builder);
    FLARE_CHECK(resp.SerializePartialToZeroCopyStream(&nbos));
  } else {
    content_type = trpc::TRPC_PROTO_ENCODE;

    auto ctx = cast<ProactiveCallContext>(controller);
    trpc::RequestProtocol req;
    auto&& req_meta = meta.request_meta();

    req.set_version(trpc::TRPC_PROTO_V1);
    req.set_call_type(trpc::TRPC_UNARY_CALL);
    req.set_request_id(meta.correlation_id());
    req.set_timeout(req_meta.timeout());
    // req.set_caller(...);  // FIXME: Is this required?
    // Really wasteful. I don't see the need for `callee` given that it's
    // included in `func`.
    req.mutable_callee()->assign(ctx->method->service()->full_name().begin(),
                                 ctx->method->service()->full_name().end());
    req.set_func(Format("/{}/{}", req.callee(), ctx->method->name()));
    req.set_content_type(trpc::TRPC_PROTO_ENCODE);
    req.set_content_encoding(GetCompressType(&meta));

    // @sa: Comments on `kTracingContextKey`.
    if (!controller->GetTracingContext().empty()) {
      (*req.mutable_trans_info())[kTracingContextKey] =
          controller->GetTracingContext();
    }

    header.header_size = req.ByteSizeLong();
    NoncontiguousBufferOutputStream nbos(&builder);
    FLARE_CHECK(req.SerializePartialToZeroCopyStream(&nbos));
  }

  // Neither compression nor passing raw bytes are supported if JSON was
  // requested. This should do little harm in practice, as JSON is neither
  // performant nor space efficient anyway.
  if (content_type == trpc::TRPC_JSON_ENCODE &&
      msg->msg_or_buffer.index() == 1) {
    std::string json_str;
    google::protobuf::util::JsonPrintOptions opts;
    opts.preserve_proto_field_names = true;
    FLARE_CHECK(google::protobuf::util::MessageToJsonString(
                    *std::get<1>(msg->msg_or_buffer), &json_str, opts)
                    .ok(),
                "Failed to serialize Protocol Buffers message");
    header.total_size =
        sizeof(TrpcHeader) + header.header_size + json_str.size();
    builder.Append(json_str);
  } else {
    // Body.
    header.total_size = sizeof(TrpcHeader) + header.header_size +
                        compression::CompressBodyIfNeeded(meta, *msg, &builder);
  }

  FLARE_LOG_ERROR_IF_ONCE(
      !msg->attachment.Empty(),
      "Attachment is not supported by Trpc protocol. Dropped silently.");
  header.Encode();
  memcpy(unaligned_header, &header, sizeof(header));
  *buffer = builder.DestructiveGet();
}

}  // namespace flare::protobuf
