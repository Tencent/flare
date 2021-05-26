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

#ifndef FLARE_NET_REDIS_MESSAGE_H_
#define FLARE_NET_REDIS_MESSAGE_H_

#include "flare/rpc/protocol/message.h"
#include "flare/net/redis/redis_command.h"
#include "flare/net/redis/redis_object.h"

namespace flare::redis {

struct RedisRequest : Message {
  const RedisCommand* command;

  RedisRequest() { SetRuntimeTypeTo<RedisRequest>(); }
  std::uint64_t GetCorrelationId() const noexcept override {
    return kNonmultiplexableCorrelationId;
  }
  Type GetType() const noexcept override { return Type::Single; }
};

struct RedisResponse : Message {
  RedisObject object;

  RedisResponse() { SetRuntimeTypeTo<RedisResponse>(); }
  std::uint64_t GetCorrelationId() const noexcept override {
    return kNonmultiplexableCorrelationId;
  }
  Type GetType() const noexcept override { return Type::Single; }
};

}  // namespace flare::redis

#endif  // FLARE_NET_REDIS_MESSAGE_H_
