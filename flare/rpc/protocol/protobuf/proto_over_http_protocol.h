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

#ifndef FLARE_RPC_PROTOCOL_PROTOBUF_PROTO_OVER_HTTP_PROTOCOL_H_
#define FLARE_RPC_PROTOCOL_PROTOBUF_PROTO_OVER_HTTP_PROTOCOL_H_

#include <optional>
#include <string>

#include "flare/base/buffer.h"
#include "flare/base/maybe_owning.h"
#include "flare/rpc/protocol/protobuf/call_context.h"
#include "flare/rpc/protocol/protobuf/message.h"
#include "flare/rpc/protocol/protobuf/rpc_meta.pb.h"
#include "flare/rpc/protocol/stream_protocol.h"

namespace flare::http {

class HttpBaseMessage;

}

namespace flare::protobuf {

// Translates HTTP messages to `ProtoMessage`.
class ProtoOverHttpProtocol : public StreamProtocol {
 public:
  enum class ContentType {
    kApplicationJson,  // @sa: common/encoding/proto_json_format.h
    kProto3Json,       // @sa: protobuf/util/json_util.h
    kDebugString,
    kProtobuf
  };

  ProtoOverHttpProtocol(ContentType content_type, bool server_side);

  const Characteristics& GetCharacteristics() const override;

  const MessageFactory* GetMessageFactory() const override;
  const ControllerFactory* GetControllerFactory() const override;

  MessageCutStatus TryCutMessage(NoncontiguousBuffer* buffer,
                                 std::unique_ptr<Message>* message) override;

  bool TryParse(std::unique_ptr<Message>* message,
                Controller* controller) override;

  void WriteMessage(const Message& message, NoncontiguousBuffer* buffer,
                    Controller* controller) override;

 private:
  enum class MetaParseStatus {
    Success,
    kCriticalFieldMissing,
    kInvalidRequest,
    kInvalidHttpMethod,  // Only POST is accepted.
  };

  MessageCutStatus TryCutMessageFromChunkedEncoding(
      const std::string& header, PooledPtr<rpc::RpcMeta> meta,
      NoncontiguousBuffer* buffer, std::unique_ptr<Message>* message);

  // Fill `meta` with info from `msg` and `base_msg`.
  MetaParseStatus TryExtractRpcMeta(const std::string& header,
                                    rpc::RpcMeta* meta) const;

  // Create or find unpacking buffer for this message. Returns `nullptr` if the
  // message is not recognized.
  MaybeOwning<google::protobuf::Message> TryGetUnpackingBuffer(
      const rpc::RpcMeta& meta, ProactiveCallContext* ctx);

  // Deserialize `serialized` to `to`. Algorithm used is determined by
  // `content_type_`.
  bool TryDeserialize(const NoncontiguousBuffer& serialized,
                      google::protobuf::Message* to) const;

  // Repacks message to HTTP request.
  void WriteRequest(const ProtoMessage& message, NoncontiguousBuffer* buffer,
                    Controller* controller);

  // Repacks message to HTTP response.
  void WriteResponse(const ProtoMessage& message, NoncontiguousBuffer* buffer,
                     Controller* controller);

  // Repacks single response to HTTP response.
  void WriteStreamSingle(const ProtoMessage& message,
                         NoncontiguousBuffer* buffer, Controller* controller);

  // Repacks first stream response to HTTP response (chunked encoding).
  void WriteStreamStart(const ProtoMessage& message,
                        NoncontiguousBuffer* buffer, Controller* controller);

  // Repacks stream response to HTTP response (chunked encoding).
  void WriteStreamContinue(const ProtoMessage& message,
                           NoncontiguousBuffer* buffer, Controller* controller);

  // Repacks end-of-stream marker to HTTP response (chunked encoding).
  void WriteStreamEnd(const ProtoMessage& message, NoncontiguousBuffer* buffer,
                      Controller* controller);

  // There's No Such Thing (tm) for repacking error requests..

  // Repacks error message to HTTP response.
  void WriteError(const EarlyErrorMessage& message, NoncontiguousBuffer* buffer,
                  Controller* controller);

  // Serialize `msg` to byte stream. Algorithm used is determined by
  // `content_type_`.
  NoncontiguousBuffer SerializeMessage(const ProtoMessage& message) const;

  // If a stream was previously identified, all subsequently messages are fed to
  // this method.
  MessageCutStatus TryKeepParsingStream(NoncontiguousBuffer* buffer,
                                        std::unique_ptr<Message>* msg);

 private:
  ContentType content_type_;
  bool server_side_;
  const Characteristics* characteristics_;
  std::string expecting_content_type_;  // Determined by `content_type_`.
  std::optional<rpc::RpcMeta> current_stream_;  // `std::nullopt` is no stream
                                                // is currently being parsed.
};

}  // namespace flare::protobuf

#endif  // FLARE_RPC_PROTOCOL_PROTOBUF_PROTO_OVER_HTTP_PROTOCOL_H_
