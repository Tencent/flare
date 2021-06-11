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

#ifndef FLARE_RPC_PROTOCOL_MESSAGE_H_
#define FLARE_RPC_PROTOCOL_MESSAGE_H_

#include <cinttypes>

#include <memory>

#include "flare/base/casting.h"
#include "flare/base/enum.h"

namespace flare {

// This is the abstract representation of the message. The same message may
// have multiple different representation on wire (for example, as JSON string
// or as Protocol Buffers binary stream). Besides, the same messages may be
// presented with different headers (for example, using HTTP we may want to
// specify method name in URI, while it could be inferred when we use binary
// stream.)
class Message : public ExactMatchCastable {
 public:
  virtual ~Message() = default;

  enum class Type : std::uint64_t {
    // There is no stream involved.
    Single,

    // This message belongs to a stream.
    //
    // If a message belongs to a streaming method (even if the caller / service
    // only want to send a single message in its request / response), it should
    // use `Stream` (with both `StartOfStream` and `EndOfStream` set.).
    Stream,

    // The following enumerators are used as bitwise-or'ed flags.
    //
    // I think we should use a dedicated `GetFlags()` for them to leave `Type`
    // pure.

    // If set, this message is the first one in a stream.
    //
    // This is not always required for client-side. There are protocols in use
    // which have no obvious mark for the first message in a stream. Therefore
    // not requiring this flag is a must for supporting such protocols.
    //
    // DO NOT RELY ON THIS FLAG AT CLIENT-SIDE.
    StartOfStream = 1ULL << 62,

    // If set, this message is the last one in a stream.
    //
    // Note that it's not always required for such a message to be present, in
    // case the receiver-side application code has its own way to identify
    // end-of-stream, it can simply close `InputStream` on its side to notify
    // the framework that further messages (if any) should be dropped.
    //
    // However, in case application-level protocol does not have an
    // end-of-stream indicator, this flag helps the application to determine
    // when the stream ends.
    //
    // DO NO RELY ON THIS FLAG BEING PRESENT.
    EndOfStream = 1ULL << 63
  };

  // Value of this constant does not matter, as there's only one call on the
  // connection anyway.
  //
  // Note that 0 is not usable here as we use 0 as a guard value.
  //
  // FIXME: "Multiplex[a]ble" or "Multiplex[i]ble"? The former seems a bit more
  // common.
  static constexpr auto kNonmultiplexableCorrelationId = 1;

  // Correlation ID uniquely identifies a call.
  //
  // If multiplexing is no supported by the underlying protocol, return
  // `kNonmultiplexableCorrelationId`.
  virtual std::uint64_t GetCorrelationId() const noexcept = 0;

  // Returns type of this message. @sa: `Type`.
  virtual Type GetType() const noexcept = 0;
};

// Factory for producing "special" messages.
class MessageFactory {
 public:
  virtual ~MessageFactory() = default;

  enum class Type {
    // The framework asks the protocol object to create a message of this type
    // when it detects the server is overloaded. The resulting message will be
    // sent back to the caller as a response.
    //
    // Server side.
    Overloaded,

    // The framework creates a message of this type if it has detected too many
    // "overloaded" response from the callee. In this case, the framework
    // deliberately fails RPC requests made by the program for some time, to
    // prevent further pressure to the callee.
    //
    // Client side.
    CircuitBroken
  };

  // This method is permitted to "fail", i.e. returning `nullptr`. This won't
  // lead to a disastrous result. The caller is required to handle `nullptr`
  // gracefully (in most case this leads to a similar situation as "RPC
  // timeout".).
  virtual std::unique_ptr<Message> Create(Type type,
                                          std::uint64_t correlation_id,
                                          bool stream) const = 0;

  // A predefined factory that always returns `nullptr`.
  static const MessageFactory* null_factory;
};

FLARE_DEFINE_ENUM_BITMASK_OPS(Message::Type);

}  // namespace flare

#endif  // FLARE_RPC_PROTOCOL_MESSAGE_H_
