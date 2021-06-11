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

#ifndef FLARE_NET_REDIS_REDIS_CLIENT_H_
#define FLARE_NET_REDIS_REDIS_CLIENT_H_

#include <chrono>
#include <string>

#include "gflags/gflags_declare.h"

#include "flare/base/future.h"
#include "flare/base/internal/lazy_init.h"
#include "flare/base/internal/time_view.h"
#include "flare/base/maybe_owning.h"
#include "flare/net/redis/redis_channel.h"
#include "flare/net/redis/redis_command.h"
#include "flare/net/redis/redis_object.h"

DECLARE_int32(flare_redis_client_default_timeout_ms);

namespace flare {

// This class helps you make requests to Redis server.
//
// For the moment it is heavy to create / destroy. It's suggested to reuse Redis
// client if possible.
//
// CAUTION: Do not rely on relative order between concurrent requests. There's
// absolutely NO guaranteed about order. If order should be enforced, use
// `RedisPipelinedClient` instead.
class RedisClient {
 public:
  struct Options {
    std::chrono::nanoseconds timeout =
        std::chrono::milliseconds(FLAGS_flare_redis_client_default_timeout_ms);
  };

  // Use an existing Redis channel to execute commands.
  explicit RedisClient(
      MaybeOwningArgument<RedisChannel> channel,
      const Options& options = internal::LazyInitConstant<Options>());

  // Opens a channel implicitly.
  RedisClient(const std::string& uri,
              const Options& options = internal::LazyInitConstant<Options>());
  ~RedisClient();

  // Executes Redis command.
  //
  // On error, a `RedisError` object with `X-xxx` (`X-CONN`, for example) error
  // category is returned.
  RedisObject Execute(const RedisCommand& command,
                      internal::SteadyClockView timeout = {});
  Future<RedisObject> AsyncExecute(const RedisCommand& command,
                                   internal::SteadyClockView timeout = {});

 private:
  Options options_;

  MaybeOwning<RedisChannel> channel_;
};

// TODO(luobogao): Introduce `RedisPipelinedClient`. This client owns a Redis
// connection exclusively. This allows us to implement command pipelining and
// Pub/Sub programming interface.

}  // namespace flare

#endif  // FLARE_NET_REDIS_REDIS_CLIENT_H_
