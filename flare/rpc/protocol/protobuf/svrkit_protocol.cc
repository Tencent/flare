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

#include "flare/rpc/protocol/protobuf/svrkit_protocol.h"

#include <optional>
#include <string>
#include <utility>

#include "flare/base/buffer.h"
#include "flare/base/buffer/zero_copy_stream.h"
#include "flare/base/down_cast.h"
#include "flare/base/endian.h"
#include "flare/rpc/protocol/protobuf/call_context.h"
#include "flare/rpc/protocol/protobuf/call_context_factory.h"
#include "flare/rpc/protocol/protobuf/compression.h"
#include "flare/rpc/protocol/protobuf/message.h"
#include "flare/rpc/protocol/protobuf/rpc_meta.pb.h"
#include "flare/rpc/protocol/protobuf/rpc_options.h"
#include "flare/rpc/protocol/protobuf/service_method_locator.h"

namespace flare::protobuf {

FLARE_RPC_REGISTER_CLIENT_SIDE_STREAM_PROTOCOL_ARG("svrkit", SvrkitProtocol,
                                                   false);
FLARE_RPC_REGISTER_SERVER_SIDE_STREAM_PROTOCOL_ARG("svrkit", SvrkitProtocol,
                                                   true);

namespace {

namespace svrkit {

// Clang complains about unused variable if we define these constants as
// variable.
enum SvrkitStatus {
  COMM_OK = 0,
  COMM_ERR_GENERAL = -1,
  COMM_ERR_PARA = -2,
  COMM_ERR_NOMEM = -3,
  COMM_ERR_FILEIO = -4,
  COMM_ERR_SQLDB = -5,
  COMM_ERR_DATA = -6,
  COMM_ERR_NETIO = -7,
  COMM_ERR_SERVERBUSY = -8,
  COMM_ERR_SIGABORT = -9,
  COMM_ERR_OVERSIZE = -10,
  COMM_ERR_SERVERMASKED = -11,
  COMM_ERR_SERVERREADONLY = -12,
  COMM_ERR_OUTOFRANGE = -13,
  COMM_ERR_DATATICKET = -14,
  COMM_ERR_ACCIP_ZK_REFUSE = -15,
  COMM_ERR_MMLAS_REFUSE = -16,
  COMM_ERR_SAFE_KEY_AGENT_SYS_ERR = -17,
  COMM_ERR_PRE_POST_NOT_IMPLEMENTED = -18,
  COMM_ERR_SOCKETOPEN = -201,
  COMM_ERR_SOCKETREAD = -202,
  COMM_ERR_SOCKETWRITE = -203,
  COMM_ERR_SOCKETCLOSE = -204,
  COMM_ERR_SOCKETINVALID = -205,
  COMM_ERR_SOCKFASTFAILURE = -206,
  COMM_ERR_SOCKBACKENDFAIL = -207,
  COMM_ERR_SOCKMAXCONN = -208,
  COMM_ERR_SOCKMAXACCQUE = -209,
  COMM_ERR_SOCKMAXINQUE = -210,
  COMM_ERR_REQUNCOMPRESS = -211,
  COMM_ERR_BUSINESSREJECT = -212,
  COMM_ERR_PERCENTBLOCKMACHINE = -213,
  COMM_ERR_ROUTEERR = -214,
  COMM_ERR_PARSEPROTOFAIL = -215,
  COMM_PROTOCOLINVALID = -301,
  COMM_ERR_SVRACTIVEREJECT = -601,
  COMM_ERR_SVRCALLSTEPREJECT = -602,
  COMM_ERR_TRANSFERTIMEOUT = -603,
  COMM_ERR_BLOCKMACHINE = -604,
  COMM_ERR_MAX_NEGATIVE = -1000  // Declared by us.
};

// Adapted from `common/spp/channel/svrkit_channel.cc`.

// Wire format:
//
// (Note that we merged `total_size` into `SvrkitHeader`.
//
// - Request
//
//   [size (4B)][SvrkitHeader][protobuf-encoded] (`segs_present_in_req` not set)
//
//   [size (4B)][SvrkitHeader][SvrkitSegmentHeader][protobuf-encoded]["END"] ...
//   [SvrkitSegmentHeader][Cookie]["END"] (`segs_present_in_req` set)
//
// - Reply
//
//   [size (4B)][SvrkitHeader][protobuf-encoded]

// Message header.
//
// @sa: `wrpc/common/global.h`. Svrkit designs its protocol is a REALLY dirty
// way.
struct SvrkitHeader {
  // This field **should** be filled with zero but most (all?) of the services
  // used it as a part of identifier (in conjunction with `cmd_id`) for
  // identifying method being called.
  std::uint16_t magic;
  std::uint8_t version;         // Always zero.
  std::uint8_t header_size;     // Size of this header (32 bytes).
  std::uint32_t body_size;      // Size of body
  std::uint16_t cmd_id;         // Method being called (@sa: `magic`).
  std::uint16_t checksum;       // Checksum of the header.
  std::uint32_t x_forward_for;  // For HTTP proxied request, always zero.
  std::uint8_t dirty_flags[4];  // Rather dirty. See `Get/SetDirtyFlagXxx`.
  std::uint32_t caller_uin;
  std::int32_t status;
  std::uint8_t always_one;
  std::uint8_t reserved1;            // Reserved, always zero.
  std::uint8_t segs_present_in_req;  // Segments present if set. Not applicable
                                     // to server side (besides, it's used for
                                     // other purpose on client side.)
  std::uint8_t verbose_log;  // If set, log should be printed in verbose level.
};

static_assert(sizeof(SvrkitHeader) == 32);

// Test if the body is compressed.
//
// For the moment only snappy is supported.
bool GetDirtyFlagCompressed(const SvrkitHeader& header, bool is_request) {
  // Well, different byte is used, depending on whether it's a request.
  auto&& byte = is_request ? header.dirty_flags[2] : header.dirty_flags[1];
  auto bit = is_request ? 2 : 1;  // ....

  // Per Svrkit's definition, these flags are defined as bit-fields. I'm not
  // sure if they're serious about this, considering that memory layout of
  // bit-fields is not specified by the standard.
  return byte & (1 << bit);
}

void SetDirtyFlagCompressed(bool is_request, SvrkitHeader* header) {
  // @sa: `GetDirtyFlagCompressed`
  auto&& byte = is_request ? header->dirty_flags[2] : header->dirty_flags[1];
  auto bit = is_request ? 2 : 1;  // ....
  byte |= (1 << bit);
}

// Test if compression can (but not required to) be applied to the response.
//
// Only applicable to request message.
bool GetDirtyFlagCompressionAllowed(const SvrkitHeader& header) {
  return header.dirty_flags[2] & (1 << 1);
}

void SetDirtyFlagCompressionAllowed(SvrkitHeader* header) {
  header->dirty_flags[2] |= (1 << 1);
}

// If `SvrkitHeader.segs_present_in_req` is set, two segments, each prefixed
// with a header defined below, are present.
struct SvrkitSegmentHeader {
  std::uint32_t type;  // 1: Protobuf-encoded; 2: Cookie.
  std::uint32_t size;  // The header itself is not counted. End-of-segment
                       // marker ("END") is counted.
  // Data follows.
};

static_assert(sizeof(SvrkitSegmentHeader) == 8);

std::uint16_t SvrkitHeaderChecksum(const SvrkitHeader* p) {
  std::uint64_t sum = 0;
  auto data = reinterpret_cast<const std::uint16_t*>(p);
  auto len = sizeof(SvrkitHeader) / 2;
  auto mod = sizeof(SvrkitHeader) % 2;

  for (std::size_t i = 0; i < len; ++i) {
    sum += data[i];
  }
  if (mod == 1) {
    // This branch is never taken. But we keep this code nonetheless (it's
    // adapted from `common/...`.).
    auto temp = reinterpret_cast<const char*>(p)[sizeof(SvrkitHeader) - 1];
    sum += temp;
  }
  sum = (sum >> 16) + (sum & 0xffff);
  sum += (sum >> 16);

  return ~static_cast<std::uint16_t>(sum);
}

}  // namespace svrkit

}  // namespace

namespace {

std::optional<std::pair<std::int32_t, std::int32_t>> TryGetSvrkitMethodKey(
    const google::protobuf::MethodDescriptor* method) {
  auto service = method->service();
  auto sid = TryGetSvrkitServiceId(service), mid = TryGetSvrkitMethodId(method);
  if (sid && mid) {
    return std::pair(*sid, *mid);
  }
  return {};
}

void RegisterMethodCallback(const google::protobuf::MethodDescriptor* method) {
  if (auto opt = TryGetSvrkitMethodKey(method)) {
    ServiceMethodLocator::Instance()->RegisterMethod(protocol_ids::svrkit,
                                                     method, *opt);
  }
}

void DeregisterMethodCallback(
    const google::protobuf::MethodDescriptor* method) {
  if (auto opt = TryGetSvrkitMethodKey(method)) {
    ServiceMethodLocator::Instance()->DeregisterMethod(protocol_ids::svrkit,
                                                       method);
  }
}

}  // namespace

FLARE_RPC_PROTOCOL_PROTOBUF_REGISTER_METHOD_PROVIDER(RegisterMethodCallback,
                                                     DeregisterMethodCallback);

namespace {

struct OnWireMessage : Message {
  OnWireMessage() { SetRuntimeTypeTo<OnWireMessage>(); }
  // Multiplexing is not supported by Svrkit protocol. Always returns zero.
  std::uint64_t GetCorrelationId() const noexcept override {
    return kNonmultiplexableCorrelationId;
  }
  Type GetType() const noexcept override { return Type::Single; }

  svrkit::SvrkitHeader header;
  std::string cookie;  // Not passed to outside for now.
  NoncontiguousBuffer payload;
};

// Convert native endian to on-wire endian.
//
// Checksum of header is filled as well.
void PrepareForWritingOnWire(svrkit::SvrkitHeader* header) {
  ToBigEndian(&header->magic);
  ToBigEndian(&header->version);
  ToBigEndian(&header->header_size);
  ToBigEndian(&header->body_size);
  ToBigEndian(&header->cmd_id);
  // `header->checksum` is filled later (see below).
  ToBigEndian(&header->x_forward_for);
  // `header->reserved0` is not touched.
  ToBigEndian(&header->caller_uin);
  ToBigEndian(&header->status);
  ToBigEndian(&header->always_one);
  ToBigEndian(&header->reserved1);
  ToBigEndian(&header->segs_present_in_req);
  ToBigEndian(&header->verbose_log);
  header->checksum = ToBigEndian<std::uint16_t>(SvrkitHeaderChecksum(header));
}

[[maybe_unused]] void PrepareForWritingOnWire(
    svrkit::SvrkitSegmentHeader* header) {
  ToBigEndian(&header->type);
  ToBigEndian(&header->size);
}

// Convert on-wire endian to native endian.
//
// Checksum is not checked.
void PrepareForReadingFromWire(svrkit::SvrkitHeader* header) {
  FromBigEndian(&header->magic);
  FromBigEndian(&header->version);
  FromBigEndian(&header->header_size);
  FromBigEndian(&header->body_size);
  FromBigEndian(&header->cmd_id);
  FromBigEndian(&header->checksum);  // Not checked.
  FromBigEndian(&header->x_forward_for);
  // `header->reserved0` is not touched.
  FromBigEndian(&header->caller_uin);
  FromBigEndian(&header->status);
  FromBigEndian(&header->always_one);
  FromBigEndian(&header->reserved1);
  FromBigEndian(&header->segs_present_in_req);
  FromBigEndian(&header->verbose_log);
}

void PrepareForReadingFromWire(svrkit::SvrkitSegmentHeader* header) {
  FromBigEndian(&header->type);
  FromBigEndian(&header->size);
}

// Mapping from rpc::STATUS_XXX to Svrkit status.
constexpr std::pair<int, int> kRpcStatusToSvrkitStatus[] = {
    {rpc::STATUS_SUCCESS, svrkit::COMM_OK},
    {rpc::STATUS_CHANNEL_SHUTDOWN, svrkit::COMM_ERR_NETIO},
    {rpc::STATUS_FAIL_TO_CONNECT, svrkit::COMM_ERR_NETIO},
    {rpc::STATUS_SERIALIZE_REQUEST, svrkit::COMM_ERR_DATA},
    {rpc::STATUS_PARSE_REQUEST, svrkit::COMM_ERR_PARSEPROTOFAIL},
    {rpc::STATUS_SERIALIZE_RESPONSE, svrkit::COMM_ERR_DATA},
    {rpc::STATUS_PARSE_RESPONSE, svrkit::COMM_ERR_PARSEPROTOFAIL},
    {rpc::STATUS_INVALID_METHOD_NAME, svrkit::COMM_ERR_PARA},
    {rpc::STATUS_SERVICE_NOT_FOUND, svrkit::COMM_ERR_PARA},
    {rpc::STATUS_METHOD_NOT_FOUND, svrkit::COMM_ERR_PARA},
    {rpc::STATUS_OVERLOADED, svrkit::COMM_ERR_SERVERBUSY},
    {rpc::STATUS_INVALID_TRANSFER_MODE, svrkit::COMM_ERR_NETIO},
    {rpc::STATUS_OUT_OF_SERVICE, svrkit::COMM_ERR_SVRACTIVEREJECT},
    {rpc::STATUS_GET_ROUTE, svrkit::COMM_ERR_ROUTEERR},
    {rpc::STATUS_GET_ROUTE_ALL_DISABLED, svrkit::COMM_ERR_ROUTEERR},
    {rpc::STATUS_TIMEOUT, svrkit::COMM_ERR_TRANSFERTIMEOUT},
    {rpc::STATUS_NO_PEER, svrkit::COMM_ERR_ROUTEERR},
    {rpc::STATUS_FAILED, svrkit::COMM_ERR_GENERAL},
    {rpc::STATUS_MALFORMED_DATA, svrkit::COMM_ERR_DATA},
    {rpc::STATUS_INVALID_CHANNEL, svrkit::COMM_ERR_NETIO},

    // `STATUS_FROM_USER` / `STATUS_FAILED` are treated specially to prevent
    // Svrkit recognizing them as framework error.
    {rpc::STATUS_FAILED, 0x7ffff'ffff},
    {rpc::STATUS_FROM_USER, 0x7ffff'fffe},

    // Anything else are mapped to `COMM_ERR_GENERAL` by default.
};

// From Svrkit status to rpc::STATUS_XXX.
//
// As the error code is not a one-to-one mapping, we cannot simply do a reverse
// mapping of the above one.
constexpr std::pair<int, int> kSvrkitStatusToRpcStatus[] = {
    {svrkit::COMM_OK, rpc::STATUS_SUCCESS},
    {svrkit::COMM_ERR_DATA, rpc::STATUS_MALFORMED_DATA},
    {svrkit::COMM_ERR_SERVERBUSY, rpc::STATUS_OVERLOADED},
    {svrkit::COMM_ERR_ROUTEERR, rpc::STATUS_NO_PEER},
    {svrkit::COMM_ERR_PARSEPROTOFAIL, rpc::STATUS_MALFORMED_DATA},
    {svrkit::COMM_PROTOCOLINVALID, rpc::STATUS_MALFORMED_DATA},
    {svrkit::COMM_ERR_SVRACTIVEREJECT, rpc::STATUS_OUT_OF_SERVICE},
    {svrkit::COMM_ERR_TRANSFERTIMEOUT, rpc::STATUS_TIMEOUT},
    // Anything else are mapped to `rpc::STATUS_FAILED` by default.
};

// Converts rpc::STATUS_XXX to status code recognized by Svrkit, in a best
// effort fashion.
int ToSvrkitStatus(int status) {
  static constexpr auto kMapping = [] {
    std::array<int, rpc::STATUS_RESERVED_MAX> mapping{};

    for (auto&& e : mapping) {
      e = svrkit::COMM_ERR_GENERAL;
    }
    for (auto&& [s, t] : kRpcStatusToSvrkitStatus) {
      mapping[s] = t;
    }
    return mapping;
  }();

  FLARE_CHECK_GE(status, 0);

  // System status codes.
  if (status < kMapping.size()) {
    return kMapping[status];
  }
  FLARE_CHECK_NE(
      status, rpc::STATUS_RESERVED_MAX,
      "`rpc::STATUS_RESERVED_MAX` should never be used in practice.");

  // Otherwise keep it as-is -- It's a user-defined status code.
  FLARE_CHECK_GT(status, rpc::STATUS_RESERVED_MAX);
  return status - rpc::STATUS_RESERVED_MAX;
}

// From Svrkit status to rpc::STATUS_XXX.
int FromSvrkitStatus(int status) {
  static constexpr auto kMapping = [] {
    std::array<int, -svrkit::COMM_ERR_MAX_NEGATIVE> mapping{};

    for (auto&& e : mapping) {
      e = rpc::STATUS_FAILED;
    }
    for (auto&& [s, t] : kSvrkitStatusToRpcStatus) {
      mapping[-s /* Negative to positive */] = t;
    }
    return mapping;
  }();

  // System status codes.
  if (status <= 0) {
    auto abs = std::abs(status);
    if (abs < kMapping.size()) {
      return kMapping[abs];
    }
    FLARE_LOG_ERROR_ONCE("Unexpected: Svrkit status {}.", status);
    return rpc::STATUS_FAILED;
  }

  // User-defined status otherwise.
  return status + rpc::STATUS_RESERVED_MAX;
}

std::uint64_t GetCompressionAlgorithmsAllowed(
    const svrkit::SvrkitHeader& header) {
  std::uint64_t result = 1 << rpc::COMPRESSION_ALGORITHM_NONE;
  if (svrkit::GetDirtyFlagCompressionAllowed(header)) {
    result |= 1 << rpc::COMPRESSION_ALGORITHM_SNAPPY;
  }
  return result;
}

bool ConsiderEnableCompression(rpc::RpcMeta* meta) {
  if (!meta->has_compression_algorithm()) {
    return false;
  }
  auto compression = meta->compression_algorithm();
  switch (compression) {
    case rpc::COMPRESSION_ALGORITHM_NONE:
      return false;
    case rpc::COMPRESSION_ALGORITHM_SNAPPY:
      return true;
    default:
      meta->clear_compression_algorithm();
      FLARE_LOG_WARNING_EVERY_SECOND(
          "Svrkit does not support compression algorithm #{}", compression);
      return false;
  }
}

enum class ProtocolIdentification { Positive, Negative, MoreDataRequired };

ProtocolIdentification TryIdentifySvrkitSegment(NoncontiguousBuffer* buffer) {
  svrkit::SvrkitSegmentHeader seg_header;
  if (buffer->ByteSize() < sizeof(seg_header)) {
    FLARE_VLOG(1, "Partial segment found.");
    return ProtocolIdentification::MoreDataRequired;
  }
  FlattenToSlow(*buffer, &seg_header, sizeof(seg_header));
  PrepareForReadingFromWire(&seg_header);
  buffer->Skip(sizeof(seg_header));
  if (seg_header.type != 1 && seg_header.type != 2) {  // PB or Cookie.
    FLARE_LOG_WARNING_EVERY_SECOND("Unexpected segment type #{} found.",
                                   seg_header.type);
    return ProtocolIdentification::Negative;
  }
  if (seg_header.size < 3 /* "END" */) {
    FLARE_LOG_WARNING_EVERY_SECOND("Segment size is too small to be legal.");
    return ProtocolIdentification::Negative;
  }
  if (buffer->ByteSize() < seg_header.size) {
    FLARE_VLOG(1, "Partial segment found.");
    return ProtocolIdentification::MoreDataRequired;
  }
  buffer->Skip(seg_header.size - 3);
  if (FlattenSlow(*buffer, 3) != "END") {
    FLARE_LOG_WARNING_EVERY_SECOND("Invalid end-of-segment marker found.");
    return ProtocolIdentification::Negative;
  }
  buffer->Skip(3);
  return ProtocolIdentification::Positive;
}

ProtocolIdentification TryIdentifySvrkitPacket(
    const NoncontiguousBuffer& buffer) {
  // We shouldn't be called otherwise.
  FLARE_CHECK_GE(buffer.ByteSize(),
                 4 /* size */ + sizeof(svrkit::SvrkitHeader));

  struct SuperHeader {
    std::uint32_t total_size;
    svrkit::SvrkitHeader header;
  } sized_hdr;
  // Don't padding.
  static_assert(sizeof(SuperHeader) == 4 + sizeof(svrkit::SvrkitHeader));

  FlattenToSlow(buffer, &sized_hdr, sizeof(sized_hdr));
  FromBigEndian(&sized_hdr.total_size);
  auto&& header = sized_hdr.header;
  PrepareForReadingFromWire(&sized_hdr.header);

  // Basic sanity checks.
  if (header.header_size != sizeof(svrkit::SvrkitHeader) ||
      sizeof(svrkit::SvrkitHeader) + header.body_size != sized_hdr.total_size ||
      header.always_one != 1) {
    // No log is printed here so as not to be too verbose.
    return ProtocolIdentification::Negative;
  }

  // Let's see if we recognizes this method first.
  //
  // Strictly speaking we should handle "method not found" differently from
  // "protocol mismatch". But there isn't a reliable way to detect Svrkit
  // protocol, and a mismatch protocol can look quite indistinguishable from
  // method-not-found..
  //
  // Either situation is an error, anyway.
  auto desc = ServiceMethodLocator::Instance()->TryGetMethodDesc(
      protocol_ids::svrkit, {header.magic, header.cmd_id});
  if (!desc) {
    FLARE_LOG_WARNING_EVERY_SECOND(
        "Unrecognized magic / cmd_id ({}, {}), not sure if it's a "
        "method-not-found error or a protocol mismatch. Ignoring the packet.",
        header.magic, header.cmd_id);
    return ProtocolIdentification::Negative;
  }

  // We don't test if this is a server-side packet: We're only called for
  // identifying server-side packet anyway.
  if (header.segs_present_in_req) {
    auto copy = buffer;  // Slow, but only once for each connection.
    FLARE_CHECK_GE(copy.ByteSize(), 4 /* size*/ + sizeof(header));
    copy.Skip(4 + sizeof(header));

    // Exactly two segments should appear, so we call `TryIdentifySvrkitSegment`
    // twice.
    auto first_part = TryIdentifySvrkitSegment(&copy);
    if (first_part != ProtocolIdentification::Positive) {
      return first_part;
    }
    return TryIdentifySvrkitSegment(&copy);
  }

  return ProtocolIdentification::Positive;
}

bool TryReadNextSegment(NoncontiguousBuffer* buffer, std::string* cookie,
                        NoncontiguousBuffer* payload) {
  if (buffer->ByteSize() < sizeof(svrkit::SvrkitSegmentHeader)) {
    // Unless there's a protocol error, this can't be the case.
    FLARE_LOG_WARNING_EVERY_SECOND("Unexpected: No enough data.");
    return false;
  }

  // The header.
  svrkit::SvrkitSegmentHeader header;
  FlattenToSlow(*buffer, &header, sizeof(header));
  PrepareForReadingFromWire(&header);
  buffer->Skip(sizeof(header));
  if (buffer->ByteSize() < header.size) {
    FLARE_LOG_WARNING_EVERY_SECOND("Unexpected: Partial segment?");
    return false;
  }

  if (header.size < sizeof(svrkit::SvrkitSegmentHeader) + 3 /* "END" */) {
    FLARE_LOG_WARNING_EVERY_SECOND(
        "Unexpected: Total size of segment is smaller than a header?");
    return false;
  }
  auto rest = header.size - 3;  // "END"

  // Let's see the type of the segment.
  if (header.type == 1) {  // Protobuf payload.
    *payload = buffer->Cut(rest);
  } else if (header.type == 2) {  // Cookie.
    *cookie = FlattenSlow(*buffer, rest);
    buffer->Skip(rest);
  } else {
    FLARE_LOG_WARNING_EVERY_SECOND("Unexpected: Unrecognized segment type #{}.",
                                   header.type);
    return false;
  }

  // End-of-segment marker.
  if (FlattenSlow(*buffer, 3) != "END") {
    FLARE_LOG_WARNING_EVERY_SECOND(
        "Unexpected: No 'END' present after segment.");
    return false;
  }
  buffer->Skip(3);  // "END"
  return true;
}

StreamProtocol::Characteristics characteristics = {.name = "Svrkit",
                                                   .not_multiplexable = true};

}  // namespace

const StreamProtocol::Characteristics& SvrkitProtocol::GetCharacteristics()
    const {
  return characteristics;
}

const MessageFactory* SvrkitProtocol::GetMessageFactory() const {
  return &error_message_factory;
}

const ControllerFactory* SvrkitProtocol::GetControllerFactory() const {
  return &passive_call_context_factory;
}

StreamProtocol::MessageCutStatus SvrkitProtocol::TryCutMessage(
    NoncontiguousBuffer* buffer, std::unique_ptr<Message>* message) {
  if (buffer->ByteSize() < 4 /* size */ + sizeof(svrkit::SvrkitHeader)) {
    return MessageCutStatus::NotIdentified;
  }

  // Only if we haven't recognized the protocol on this connection, we'll try to
  // identify the protocol. Otherwise we'll simply raise an error if some other
  // protocol is received.
  //
  // This check is not required for client-side connection. For client-side,
  // it's us who determine which protocol is running on the wire.
  if (FLARE_UNLIKELY(
          !skip_protocol_identification_.load(std::memory_order_relaxed) &&
          server_side_)) {
    auto identification = TryIdentifySvrkitPacket(*buffer);
    if (identification == ProtocolIdentification::Negative) {
      return MessageCutStatus::ProtocolMismatch;
    } else if (identification == ProtocolIdentification::MoreDataRequired) {
      return MessageCutStatus::NeedMore;
    }
    FLARE_CHECK(identification == ProtocolIdentification::Positive);
    skip_protocol_identification_.store(true, std::memory_order_relaxed);
  }

  // Let's see if the packet is complete.
  std::uint32_t total_size;
  FlattenToSlow(*buffer, &total_size, sizeof(total_size));
  FromBigEndian(&total_size);
  if (buffer->ByteSize() < sizeof(total_size) + total_size) {
    return MessageCutStatus::NeedMore;
  }
  buffer->Skip(sizeof(total_size));

  // Parse the header first.
  svrkit::SvrkitHeader header;
  FlattenToSlow(*buffer, &header, sizeof(header));
  PrepareForReadingFromWire(&header);

  // Only permanent error and success are allowed to be returned since now.

  // Segments in this packet.
  std::string cookie;
  NoncontiguousBuffer payload;

  if (server_side_ && header.segs_present_in_req) {
    auto packet = buffer->Cut(total_size);
    packet.Skip(sizeof(svrkit::SvrkitHeader));
    // Exactly two seguments should appear, therefore we read segment twice.
    if (!TryReadNextSegment(&packet, &cookie, &payload) ||
        !TryReadNextSegment(&packet, &cookie, &payload)) {
      FLARE_LOG_WARNING_EVERY_SECOND(
          "Svrkit protocol: Failed to read segment (cookie or payload).");
      return MessageCutStatus::Error;
    }
    if (!packet.Empty()) {
      FLARE_LOG_WARNING_EVERY_SECOND(
          "Svrkit protocol: Extra bytes after segments.");
      return MessageCutStatus::Error;
    }
  } else {
    // No cookie is present, easy case.
    FLARE_CHECK(!header.segs_present_in_req);
    buffer->Skip(sizeof(svrkit::SvrkitHeader));
    payload = buffer->Cut(header.body_size);
  }

  // Save the header & payload for parse later.
  auto msg = std::make_unique<OnWireMessage>();
  msg->header = header;
  msg->cookie = std::move(cookie);
  msg->payload = std::move(payload);

  *message = std::move(msg);
  return MessageCutStatus::Cut;
}

bool SvrkitProtocol::TryParse(std::unique_ptr<Message>* message,
                              Controller* controller) {
  auto&& on_wire = cast<OnWireMessage>(message->get());
  auto meta = object_pool::Get<rpc::RpcMeta>();
  MaybeOwning<google::protobuf::Message> unpack_to;
  bool accept_msg_in_bytes;
  auto parsed = std::make_unique<ProtoMessage>();

  meta->set_correlation_id(Message::kNonmultiplexableCorrelationId);
  meta->set_method_type(rpc::METHOD_TYPE_SINGLE);
  if (svrkit::GetDirtyFlagCompressed(
          on_wire->header, server_side_ ? true : false /* is_request */)) {
    meta->set_compression_algorithm(rpc::COMPRESSION_ALGORITHM_SNAPPY);
  }

  if (server_side_) {
    auto&& req_meta = *meta->mutable_request_meta();
    auto&& desc = ServiceMethodLocator::Instance()->TryGetMethodDesc(
        protocol_ids::svrkit, {on_wire->header.magic, on_wire->header.cmd_id});
    if (!desc) {  // Unknown method then.
      FLARE_LOG_WARNING_EVERY_SECOND("Unrecognized svrkit magic/cmd: {}/{}.",
                                     on_wire->header.magic,
                                     on_wire->header.cmd_id);
      *message = std::make_unique<EarlyErrorMessage>(
          meta->correlation_id(), rpc::STATUS_METHOD_NOT_FOUND,
          fmt::format("[{}/{}] (Magic/CMD) is not recognized.",
                      on_wire->header.magic, on_wire->header.cmd_id));
      return true;
    }
    req_meta.set_method_name(desc->normalized_method_name);
    req_meta.set_acceptable_compression_algorithms(
        GetCompressionAlgorithmsAllowed(on_wire->header));
    unpack_to = MaybeOwning(owning, desc->request_prototype->New());
    accept_msg_in_bytes = false;
  } else {
    auto&& resp_meta = *meta->mutable_response_meta();
    resp_meta.set_status(FromSvrkitStatus(on_wire->header.status));
    auto ctx = cast<ProactiveCallContext>(controller);
    if (ctx->accept_response_in_bytes) {
      accept_msg_in_bytes = true;
    } else {
      unpack_to = ctx->GetOrCreateResponse();
      accept_msg_in_bytes = false;
    }
  }

  parsed->meta = std::move(meta);
  if (FLARE_UNLIKELY(accept_msg_in_bytes)) {
    parsed->msg_or_buffer = std::move(on_wire->payload);
  } else {
    // In-place decompression.
    if (!compression::DecompressBodyIfNeeded(*parsed->meta, on_wire->payload,
                                             &on_wire->payload)) {
      FLARE_LOG_WARNING_EVERY_SECOND(
          "Failed to decompress message (correlation id {}).",
          parsed->meta->correlation_id());
      return false;
    }

    NoncontiguousBufferInputStream nbis(&on_wire->payload);
    if (!unpack_to->ParseFromZeroCopyStream(&nbis)) {
      FLARE_LOG_WARNING_EVERY_SECOND(
          "Failed to parse message (correlation id {}).",
          meta->correlation_id());
      return false;
    }
    parsed->msg_or_buffer = std::move(unpack_to);
  }
  *message = std::move(parsed);
  return true;
}

void SvrkitProtocol::WriteMessage(const Message& message,
                                  NoncontiguousBuffer* buffer,
                                  Controller* controller) {
  auto msg = cast<ProtoMessage>(&message);
  auto&& meta = *msg->meta;

  FLARE_LOG_ERROR_IF_ONCE(
      !controller->GetTracingContext().empty() ||
          controller->IsTraceForciblySampled(),
      "Passing tracing context is not supported by Svrkit protocol.");

  svrkit::SvrkitHeader hdr = {.header_size = sizeof(svrkit::SvrkitHeader),
                              .always_one = 1};

  hdr.segs_present_in_req = 0;  // Segments are not supported yet.
  if (server_side_) {
    hdr.status = ToSvrkitStatus(meta.response_meta().status());
  } else {
    auto key =
        TryGetSvrkitMethodKey(cast<ProactiveCallContext>(controller)->method);
    FLARE_CHECK(
        key,
        "You didn't set option `svrkit_service_id` or `service_method_id` for "
        "method [{}], which are required for calling it via Svrkit protocol.",
        meta.request_meta().method_name());
    std::tie(hdr.magic, hdr.cmd_id) = *key;
    svrkit::SetDirtyFlagCompressionAllowed(&hdr);
  }
  if (ConsiderEnableCompression(&meta)) {
    svrkit::SetDirtyFlagCompressed(server_side_ ? false : true /* is_request */,
                                   &hdr);
  }

  NoncontiguousBufferBuilder nbb;
  auto size_ptr = nbb.Reserve(sizeof(std::uint32_t));
  auto hdr_ptr = nbb.Reserve(sizeof(hdr));

  hdr.body_size = compression::CompressBodyIfNeeded(meta, *msg, &nbb);
  FLARE_LOG_ERROR_IF_ONCE(
      !msg->attachment.Empty(),
      "Attachment is not supported by Svrkit protocol. Dropped silently.");

  std::uint32_t total_size =
      ToBigEndian<std::uint32_t>(hdr.header_size + hdr.body_size);
  PrepareForWritingOnWire(&hdr);
  memcpy(size_ptr, &total_size, sizeof(total_size));
  memcpy(hdr_ptr, &hdr, sizeof(hdr));
  buffer->Append(nbb.DestructiveGet());
}

}  // namespace flare::protobuf
