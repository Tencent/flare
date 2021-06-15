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

#include "flare/net/hbase/hbase_client_protocol.h"

#include <memory>
#include <utility>

#include "google/protobuf/util/delimited_message_util.h"

#include "flare/base/buffer/zero_copy_stream.h"
#include "flare/base/down_cast.h"
#include "flare/base/endian.h"
#include "flare/net/hbase/call_context.h"
#include "flare/net/hbase/hbase_client_controller.h"
#include "flare/net/hbase/message.h"
#include "flare/net/hbase/proto/constants.h"
#include "flare/rpc/protocol/message.h"

namespace flare::hbase {

void HbaseClientProtocol::InitializeHandshakeConfig(
    ConnectionHeader conn_header) {
  conn_header_ = std::move(conn_header);
}

const StreamProtocol::Characteristics& HbaseClientProtocol::GetCharacteristics()
    const {
  static const Characteristics cs = {.name = "HBase (server)"};
  return cs;
}

const MessageFactory* HbaseClientProtocol::GetMessageFactory() const {
  return MessageFactory::null_factory;
}

const ControllerFactory* HbaseClientProtocol::GetControllerFactory() const {
  return ControllerFactory::null_factory;
}

StreamProtocol::MessageCutStatus HbaseClientProtocol::TryCutMessage(
    NoncontiguousBuffer* buffer, std::unique_ptr<Message>* message) {
  // Because we're used on client-side, there's no need to "recognize" the
  // protocol. We just cut the messages.
  auto resp = std::make_unique<HbaseResponse>();
  auto rc = resp->TryCut(buffer);
  if (!rc) {
    return MessageCutStatus::NeedMore;
  } else if (!*rc) {
    return MessageCutStatus::Error;
  }
  if (!handshake_done_) {  // It would be better if handshaking is done
                           // separately.
    if (!resp->header.has_exception() ||
        resp->header.exception().exception_class_name() !=
            constants::kFatalConnectionException) {
      handshake_done_ = true;
    }
  }
  *message = std::move(resp);
  return MessageCutStatus::Cut;
}

bool HbaseClientProtocol::TryParse(std::unique_ptr<Message>* message,
                                   Controller* controller) {
  auto ctx = cast<ProactiveCallContext>(controller);
  auto&& resp = cast<HbaseResponse>(message->get());

  // The response message is provided by caller.
  resp->body = MaybeOwning(non_owning, ctx->response_ptr);
  if (!resp->TryParse()) {
    FLARE_LOG_WARNING_EVERY_SECOND("Cannot parse HBase response.");
    return false;
  }

  // Copy exception to `controller->rpc_controller`, if there is one.
  if (resp->header.has_exception()) {
    // FIXME: Can we move exception into controller (we copied it now)?
    ctx->client_controller->SetException(resp->header.exception());
  }
  return true;
}

void HbaseClientProtocol::WriteMessage(const Message& message,
                                       NoncontiguousBuffer* buffer,
                                       Controller* controller) {
  NoncontiguousBufferBuilder builder;

  // If the connection is newly established, we need to write preamble &
  // connection header first.
  if (!handshake_done_) {
    HbaseHandshakeHeader header;
    memcpy(header.magic, constants::kRpcHeader, constants::kRpcHeaderLength);
    header.version = constants::kRpcVersion;
    header.auth = constants::kAuthMethodSimple;
    header.conn_header_size =
        ToBigEndian<std::uint32_t>(conn_header_.ByteSizeLong());
    builder.Append(&header, sizeof(header));

    NoncontiguousBufferOutputStream nbos(&builder);
    FLARE_CHECK(conn_header_.SerializeToZeroCopyStream(&nbos));
    nbos.Flush();
  }

  cast<HbaseRequest>(message)->WriteTo(&builder);
  *buffer = builder.DestructiveGet();
}

// Registering this client protocol does not make much sense. We always create
// instances of `HbaseClientProtocol` by hand (in `HbaseChannel`.).
//
// FLARE_RPC_REGISTER_CLIENT_SIDE_STREAM_PROTOCOL("hbase", HbaseClientProtocol);

}  // namespace flare::hbase
