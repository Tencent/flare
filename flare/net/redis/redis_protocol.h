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

#ifndef FLARE_NET_REDIS_REDIS_PROTOCOL_H_
#define FLARE_NET_REDIS_REDIS_PROTOCOL_H_

#include <memory>
#include <string>

#include "flare/base/buffer.h"
#include "flare/rpc/protocol/stream_protocol.h"

namespace flare::redis {

// Implementation of Redis protocol.
//
// This is only used by non-pipelined Redis client. For pipelined client, we
// operate on connection object directly, and therefore do not need a protocol
// object.
class RedisProtocol : public StreamProtocol {
 public:
  // If set to non-empty, `AUTH password` (or `AUTH username, password`) is sent
  // to server side upon handshake.
  void SetCredential(const std::string& password);
  void SetCredential(const std::string& username, const std::string& password);

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
  bool handshake_sent_ = false;
  bool handshake_received_ = false;

  std::string username_;
  std::string password_;
};

}  // namespace flare::redis

#endif  // FLARE_NET_REDIS_REDIS_PROTOCOL_H_
