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

#include "flare/net/hbase/hbase_server_protocol.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "protobuf/descriptor.h"
#include "protobuf/util/delimited_message_util.h"

#include "flare/base/buffer/zero_copy_stream.h"
#include "flare/base/down_cast.h"
#include "flare/base/endian.h"
#include "flare/net/hbase/call_context.h"
#include "flare/net/hbase/call_context_factory.h"
#include "flare/net/hbase/hbase_server_controller.h"
#include "flare/net/hbase/message.h"
#include "flare/net/hbase/proto/constants.h"

namespace flare::hbase {

struct HbaseServerProtocol::MethodDesc {
  const google::protobuf::MethodDescriptor* desc;
  const google::protobuf::Message* request_prototype;
  const google::protobuf::Message* response_prototype;
};

struct HbaseServerProtocol::ServiceDesc {
  const google::protobuf::ServiceDescriptor* desc;
  std::unordered_map<std::string, MethodDesc> methods;
};

std::unordered_map<std::string, HbaseServerProtocol::ServiceDesc>
    HbaseServerProtocol::services_;

const StreamProtocol::Characteristics& HbaseServerProtocol::GetCharacteristics()
    const {
  static const Characteristics cs = {.name = "HBase (client)"};
  return cs;
}

const MessageFactory* HbaseServerProtocol::GetMessageFactory() const {
  return MessageFactory::null_factory;
}

const ControllerFactory* HbaseServerProtocol::GetControllerFactory() const {
  return &passive_call_context_factory;
}

StreamProtocol::MessageCutStatus HbaseServerProtocol::TryCutMessage(
    NoncontiguousBuffer* buffer, std::unique_ptr<Message>* message) {
  if (!handshake_done_) {
    if (auto status = TryCompleteHandshake(buffer);
        status != MessageCutStatus::Cut) {
      return status;
    }
    handshake_done_ = true;
  }

  auto req = std::make_unique<HbaseRequest>();
  auto rc = req->TryCut(buffer);
  if (!rc) {
    return MessageCutStatus::NeedMore;
  } else if (!*rc) {
    return MessageCutStatus::Error;
  }
  *message = std::move(req);
  return MessageCutStatus::Cut;
}

bool HbaseServerProtocol::TryParse(std::unique_ptr<Message>* message,
                                   Controller* controller) {
  auto ctx = cast<PassiveCallContext>(controller);
  auto&& req = cast<HbaseRequest>(message->get());
  const MethodDesc* method = nullptr;

  // Basic sanity checks.
  if (auto iter = service_->methods.find(req->header.method_name());
      iter != service_->methods.end()) {
    method = &iter->second;
  } else {
    FLARE_LOG_WARNING_EVERY_SECOND(
        "Method [{}] is not recognized.",
        conn_header_.service_name() + "." + req->header.method_name());
    // I didn't find an appropriate exception response to return in this case.
    //
    // The old HBase protocol implementation in our old framework doesn't return
    // an error, either.
    return false;
  }
  if (method->desc->client_streaming() || method->desc->server_streaming()) {
    FLARE_LOG_WARNING_EVERY_SECOND(
        "Unexpected: Method [{}] is declared as a streaming method, but HBase "
        "protocol does not support that.",
        method->desc->name());
    return false;
  }

  req->body = MaybeOwning(owning, method->request_prototype->New());
  if (!req->TryParse()) {
    FLARE_LOG_WARNING_EVERY_SECOND("Cannot parse HBase request.");
    return false;
  }

  ctx->service = service_->desc;
  ctx->method = method->desc;
  ctx->response.reset(method->response_prototype->New());
  // We're relying on the framework to keep us alive before the server
  // controller goes away.
  ctx->conn_header = &conn_header_;
  return true;
}

void HbaseServerProtocol::WriteMessage(const Message& message,
                                       NoncontiguousBuffer* buffer,
                                       Controller* controller) {
  NoncontiguousBufferBuilder builder;
  cast<HbaseResponse>(message)->WriteTo(&builder);
  *buffer = builder.DestructiveGet();
}

void HbaseServerProtocol::RegisterService(
    const google::protobuf::ServiceDescriptor* desc) {
  auto&& name = desc->name();
  if (auto iter = services_.find(name); iter != services_.end()) {
    FLARE_CHECK(iter->second.desc == desc, "Duplicate HBase service [{}].",
                name);
    return;
  }
  auto&& e = services_[name];
  e.desc = desc;
  for (int i = 0; i != desc->method_count(); ++i) {
    auto&& method_desc = desc->method(i);
    auto&& method = e.methods[method_desc->name()];
    method.desc = method_desc;
    method.request_prototype =
        google::protobuf::MessageFactory::generated_factory()->GetPrototype(
            method_desc->input_type());
    method.response_prototype =
        google::protobuf::MessageFactory::generated_factory()->GetPrototype(
            method_desc->output_type());
  }
}

HbaseServerProtocol::MessageCutStatus HbaseServerProtocol::TryCompleteHandshake(
    NoncontiguousBuffer* buffer) {
  HbaseHandshakeHeader header;
  if (buffer->ByteSize() < sizeof(header)) {
    return MessageCutStatus::NotIdentified;
  }
  FlattenToSlow(*buffer, &header, sizeof(header));

  // Let's check the magic to see if it's indeed HBase protocol first.
  if (memcmp(header.magic, constants::kRpcHeader,
             constants::kRpcHeaderLength)) {
    return MessageCutStatus::ProtocolMismatch;
  }

  // We treat it as an error if either:
  //
  // - RPC version mismatch, or
  // - Auth method is not supported (we support SIMPLE auth only).
  if (header.version != constants::kRpcVersion ||
      header.auth != constants::kAuthMethodSimple) {
    FLARE_LOG_WARNING_EVERY_SECOND(
        "Protocol negotiation failed: Requesting RPC version [{}], auth method "
        "[{}].",
        header.version, header.auth);
    return MessageCutStatus::Error;
  }

  // If the connection header has not been received in its entirety, we'll wait.
  header.conn_header_size =
      // Do NOT use inplace conversion here, it leads to unaligned pointer.
      FromBigEndian<std::uint32_t>(header.conn_header_size);
  if (buffer->ByteSize() <
      sizeof(HbaseHandshakeHeader) + header.conn_header_size) {
    return MessageCutStatus::NeedMore;
  }

  // Now it's safe to cut handshake data off.
  //
  // Note that we'll either complete handshake successfully, or fail
  // catastrophically (ending with in closing the connection). There's no way
  // back for "retry". So don't worry if we left the data on wire
  // "inconsistent".
  //
  // Also note that, even if we cannot read a complete request now (i.e., only
  // connection header is present but not the request itself), the framework
  // explicitly allows us to return `NeedMore` from `TryCutMessage` when we
  // mutated `buffer`. So we're still safe.
  buffer->Skip(sizeof(header));

  // Let's try extracting `ConnectionHeader`.
  {
    auto cut = buffer->Cut(header.conn_header_size);
    NoncontiguousBufferInputStream nbis(&cut);
    if (!conn_header_.ParseFromZeroCopyStream(&nbis)) {
      FLARE_LOG_WARNING_EVERY_SECOND("Failed to parse connection header.");
      return MessageCutStatus::Error;
    }
  }

  // Let's see if the service requested is known to us.
  if (auto iter = services_.find(conn_header_.service_name());
      iter != services_.end()) {
    service_ = &iter->second;
  } else {
    FLARE_LOG_WARNING_EVERY_SECOND("The requested service [{}] is unknown.",
                                   conn_header_.service_name());
    return MessageCutStatus::Error;
  }

  return MessageCutStatus::Cut;  // Not exactly.
}

FLARE_RPC_REGISTER_SERVER_SIDE_STREAM_PROTOCOL("hbase", HbaseServerProtocol);

}  // namespace flare::hbase
