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

#ifndef FLARE_NET_HBASE_HBASE_SERVER_PROTOCOL_H_
#define FLARE_NET_HBASE_HBASE_SERVER_PROTOCOL_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "google/protobuf/service.h"
#include "gtest/gtest_prod.h"

#include "flare/net/hbase/proto/rpc.pb.h"
#include "flare/rpc/protocol/stream_protocol.h"

namespace flare::hbase {

// This class implements server-side HBase protocol.
//
// @sa: `proto/README.md`
class HbaseServerProtocol : public StreamProtocol {
 public:
  const Characteristics& GetCharacteristics() const override;

  const MessageFactory* GetMessageFactory() const override;
  const ControllerFactory* GetControllerFactory() const override;

  MessageCutStatus TryCutMessage(NoncontiguousBuffer* buffer,
                                 std::unique_ptr<Message>* message) override;
  bool TryParse(std::unique_ptr<Message>* message,
                Controller* controller) override;
  void WriteMessage(const Message& message, NoncontiguousBuffer* buffer,
                    Controller* controller) override;

  // Used by `HbaseService` to register Protocol Buffers services.
  //
  // NOT thread-safe.
  static void RegisterService(const google::protobuf::ServiceDescriptor* desc);

 private:
  MessageCutStatus TryCompleteHandshake(NoncontiguousBuffer* buffer);

 private:
  FRIEND_TEST(HbaseProtocol, ClientToServer);
  FRIEND_TEST(HbaseProtocol, ServerToClient);

  struct ServiceDesc;
  struct MethodDesc;

  // When looking up for service, do not include package names.
  static std::unordered_map<std::string, ServiceDesc> services_;

  bool handshake_done_{false};

  // Fields below are set only if `handshake_done_` is set.
  ConnectionHeader conn_header_;
  const ServiceDesc* service_;
};

}  // namespace flare::hbase

#endif  // FLARE_NET_HBASE_HBASE_SERVER_PROTOCOL_H_
