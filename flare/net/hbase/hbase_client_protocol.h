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

#ifndef FLARE_NET_HBASE_HBASE_CLIENT_PROTOCOL_H_
#define FLARE_NET_HBASE_HBASE_CLIENT_PROTOCOL_H_

#include <memory>

#include "gtest/gtest_prod.h"

#include "flare/net/hbase/proto/rpc.pb.h"
#include "flare/rpc/protocol/stream_protocol.h"

namespace flare::hbase {

// This class implements client-side HBase protocol.
//
// @sa: `proto/README.md`
class HbaseClientProtocol : public StreamProtocol {
 public:
  void InitializeHandshakeConfig(ConnectionHeader conn_header);

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
  FRIEND_TEST(HbaseProtocol, ClientToServer);
  FRIEND_TEST(HbaseProtocol, ServerToClient);

  // TODO(luobogao): It would be better if the framework itself supports
  // handshaking. That way we won't need this special flag.
  bool handshake_done_{false};
  ConnectionHeader conn_header_;
};

}  // namespace flare::hbase

#endif  // FLARE_NET_HBASE_HBASE_CLIENT_PROTOCOL_H_
