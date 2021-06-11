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

#define FLARE_REDIS_CLIENT_SUPPRESS_INCLUDE_WARNING
#define FLARE_REDIS_CHANNEL_SUPPRESS_INCLUDE_WARNING

#include "flare/net/redis/redis_client.h"

#include <memory>
#include <string>
#include <utility>

#include "gflags/gflags.h"

#include "flare/fiber/future.h"
#include "flare/net/redis/redis_channel.h"

using namespace std::literals;

DEFINE_int32(flare_redis_client_default_timeout_ms, 5000,
             "Default timeout of Redis client.");

namespace flare {

RedisClient::RedisClient(MaybeOwningArgument<RedisChannel> channel,
                         const Options& options)
    : options_(options), channel_(std::move(channel)) {}

RedisClient::RedisClient(const std::string& uri, const Options& options) {
  channel_ = std::make_unique<RedisChannel>();
  options_ = options;

  // Failure is ignored. `Execute` handles connection failure for us.
  channel_->Open(uri);
}

RedisClient::~RedisClient() = default;

RedisObject RedisClient::Execute(const RedisCommand& command,
                                 internal::SteadyClockView timeout) {
  return fiber::BlockingGet(AsyncExecute(command, timeout));
}

Future<RedisObject> RedisClient::AsyncExecute(
    const RedisCommand& command, internal::SteadyClockView timeout) {
  Promise<RedisObject> p;
  auto result = p.GetFuture();
  auto cb = [p = std::move(p)](RedisObject&& obj) mutable {
    p.SetValue(std::move(obj));
  };
  auto effective_timeout = timeout.Get().time_since_epoch() != 0ns
                               ? timeout.Get()
                               : ReadSteadyClock() + options_.timeout;
  channel_->Execute(command, std::move(cb), effective_timeout);
  return result;
}

}  // namespace flare
