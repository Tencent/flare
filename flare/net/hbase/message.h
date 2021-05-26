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

#ifndef FLARE_NET_HBASE_MESSAGE_H_
#define FLARE_NET_HBASE_MESSAGE_H_

#include "flare/rpc/protocol/message.h"

#include <variant>

#include "thirdparty/protobuf/message.h"

#include "flare/base/buffer.h"
#include "flare/base/maybe_owning.h"
#include "flare/net/hbase/proto/constants.h"
#include "flare/net/hbase/proto/rpc.pb.h"

namespace flare::hbase {

// I/O buffer for `google::protobuf::Message`.
using MessageIoBuffer = std::variant<
    MaybeOwning<google::protobuf::Message>,  // Used as an input buffer.
    const google::protobuf::Message*>;       // Used as an output buffer.

// It's HBase connection preamble + byte size of connection header (@sa:
// `ConnectionHeader`). We combine them into a single C++ struct for the sake of
// programming simplicity.
//
// Placing it in `message.h` does not seem fit well, perhaps it warrants its own
// header?
struct [[gnu::packed]] HbaseHandshakeHeader {
  char magic[constants::kRpcHeaderLength];
  unsigned char version;
  unsigned char auth;

  // Connection header.
  //
  // Note that HBase's doc (`hbase/src/main/asciidoc/_chapters/rpc.adoc`) is
  // INCORRECT. The connection header is prefixed with a 4-byte big-endian
  // integer indicating its size, not variant-int.
  //
  // > This protobuf is written using writeDelimited so is prefaced by a pb
  // > varint with its serialized length...  [INCORRECT]
  //
  // @sa: `org.apache.hadoop.hbase.ipc.BlockingRpcConnection` (ctor)
  std::uint32_t conn_header_size;  // Big-endian.
};

static_assert(sizeof(HbaseHandshakeHeader) == 10);

// TODO(luobogao): Let's profile it to see if we should pool `HbaseXxx` messages
// for better performance.

// This class wraps a request message.
//
// A request consists of a `RequestHeader`, a "request param" (whose type is
// defined by the method being called), and optionally, a cell-block.
//
// There's not much sense in making it a class, we define it as a struct here.
// This structure is an implementation anyway, too much encapsulation helps
// little.
struct HbaseRequest : public Message {
  RequestHeader header;
  MessageIoBuffer body;
  NoncontiguousBuffer cell_block;

  // Cut `HbaseRequest` from byte stream.
  //
  // Note that for performance reasons, `body` / `cell_block` is NOT filled by
  // this method. It only fills `header`. You need to call `TryParse` to fill
  // the rest fields.
  //
  // Returns `true` on success, `false` on error, or `std::nullopt` if `buffer`
  // is too small.
  std::optional<bool> TryCut(NoncontiguousBuffer* buffer);

  // Fill `body` / `cell_block`.
  //
  // It's your responsibility to initialize `body` beforehand.
  bool TryParse();

  // Serialize this request to `builder`.
  //
  // Note that `body_stream` is not touched by this method. It directly
  // serialize `body` to `builder`. So, do not try serializing `body` yourself.
  void WriteTo(NoncontiguousBufferBuilder* builder) const;

  // Methods below are used by the framework.
  HbaseRequest() { SetRuntimeTypeTo<HbaseRequest>(); }
  std::uint64_t GetCorrelationId() const noexcept override {
    return header.call_id();
  }
  Type GetType() const noexcept override { return Type::Single; }

 private:
  NoncontiguousBuffer rest_bytes_;
};

// This class wraps a response message.
//
// A response consists of a `ResponseHeader`, a "response param" and an optional
// cell-block.
struct HbaseResponse : public Message {
  ResponseHeader header;
  MessageIoBuffer body;
  NoncontiguousBuffer cell_block;

  // Cut response from byte stream.
  //
  // `body` / `cell_block` is not filled. Call `TryParse` to fill them.
  //
  // Returns `true` on success, `false` on error, or `std::nullopt` if `buffer`
  // is too small.
  std::optional<bool> TryCut(NoncontiguousBuffer* buffer);

  // Fill `body` / `cell_block`.
  //
  // It's your responsibility to initialize `body` beforehand.
  bool TryParse();

  // Serialize this response to `builder`.
  void WriteTo(NoncontiguousBufferBuilder* builder) const;

  // Methods below are used by the framework.
  HbaseResponse() { SetRuntimeTypeTo<HbaseResponse>(); }
  std::uint64_t GetCorrelationId() const noexcept override {
    return header.call_id();
  }
  Type GetType() const noexcept override { return Type::Single; }

 private:
  NoncontiguousBuffer rest_bytes_;
};

}  // namespace flare::hbase

#endif  // FLARE_NET_HBASE_MESSAGE_H_
