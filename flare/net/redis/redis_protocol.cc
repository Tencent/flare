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

#include "flare/net/redis/redis_protocol.h"

#include <memory>
#include <string>
#include <utility>

#include "flare/base/casting.h"
#include "flare/net/redis/message.h"
#include "flare/net/redis/reader.h"

namespace flare::redis {

void RedisProtocol::SetCredential(const std::string& password) {
  password_ = password;
}

void RedisProtocol::SetCredential(const std::string& username,
                                  const std::string& password) {
  username_ = username;
  password_ = password;
}

const RedisProtocol::Characteristics& RedisProtocol::GetCharacteristics()
    const {
  static const Characteristics cs = {.name = "Redis",
                                     .not_multiplexable = true};
  return cs;
}

const MessageFactory* RedisProtocol::GetMessageFactory() const {
  // Not applicable to client-side protocol.
  return MessageFactory::null_factory;
}

const ControllerFactory* RedisProtocol::GetControllerFactory() const {
  return ControllerFactory::null_factory;  // Not applicable.
}

RedisProtocol::MessageCutStatus RedisProtocol::TryCutMessage(
    NoncontiguousBuffer* buffer, std::unique_ptr<Message>* message) {
  if (FLARE_UNLIKELY(!handshake_received_)) {
    // The first response is for our `AUTH` message then, we should consume this
    // message ourselves.
    RedisObject resp;
    auto rc = TryCutRedisObject(buffer, &resp);
    if (rc < 0) {
      return MessageCutStatus::Error;
    } else if (rc == 0) {
      return MessageCutStatus::NeedMore;
    }
    if (auto ptr = resp.try_as<RedisString>(); ptr && *ptr == "OK") {
      // Authenticated.
    } else {
      FLARE_LOG_ERROR_EVERY_SECOND("Credential is rejected by Redis server.");
    }

    // Either way, we don't consume more Redis message.
    //
    // In case the authentication failed, all subsequent requests (by user) will
    // fail with `NOAUTH` error.
    handshake_received_ = true;
  }

  auto resp = std::make_unique<redis::RedisResponse>();
  auto rc = TryCutRedisObject(buffer, &resp->object);
  if (rc < 0) {
    return MessageCutStatus::Error;
  } else if (rc == 0) {
    return MessageCutStatus::NeedMore;
  }
  *message = std::move(resp);
  return MessageCutStatus::Cut;
}

bool RedisProtocol::TryParse(std::unique_ptr<Message>* message,
                             Controller* controller) {
  // NOTHING here.
  return true;
}

void RedisProtocol::WriteMessage(const Message& message,
                                 NoncontiguousBuffer* buffer,
                                 Controller* controller) {
  if (!handshake_sent_) {
    if (!username_.empty()) {
      buffer->Append(RedisCommand("AUTH", username_, password_).GetBytes());
    } else if (!password_.empty()) {
      buffer->Append(RedisCommand("AUTH", password_).GetBytes());
    } else {
      // NOTHING then. Just pretend as if we've finished handshaking.
      handshake_received_ = true;
    }
    // FIXME: I think we should erase credentials from memory once they're sent.

    handshake_sent_ = true;
  }
  auto cmd = cast<RedisRequest>(message);
  buffer->Append(cmd->command->GetBytes());
}

}  // namespace flare::redis
