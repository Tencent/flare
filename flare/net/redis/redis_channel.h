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

#ifndef FLARE_NET_REDIS_REDIS_CHANNEL_H_
#define FLARE_NET_REDIS_REDIS_CHANNEL_H_

#include <chrono>
#include <memory>
#include <string>

#include "flare/base/function.h"
#include "flare/base/internal/early_init.h"
#include "flare/net/redis/redis_command.h"
#include "flare/net/redis/redis_object.h"

namespace flare {

class Message;
class Endpoint;

namespace rpc::internal {

class StreamCallGateHandle;

}  // namespace rpc::internal

namespace redis::detail {

class MockChannel;

}  // namespace redis::detail

// Represents a group of virtual connections to a Redis server cluster.
//
// No relative ordering between concurrent requests
class RedisChannel {
 public:
  struct Options {
    std::string username;
    std::string password;
    std::size_t maximum_packet_size = 64 * 1024 * 1024;
  };

  RedisChannel();
  ~RedisChannel();

  // Open the channel on construction. If we fails, any subsequent Redis command
  // executed via this channel would fail with `RedisError{"X-NOT-OPENED"}`.
  explicit RedisChannel(
      const std::string& uri,
      const Options& options = internal::EarlyInitConstant<Options>());

  // Support TCP protocol only. UNIX sockets is not supported.
  bool Open(const std::string& uri,
            const Options& options = internal::EarlyInitConstant<Options>());

  // FOR INTERNAL USE ONLY.
  static void RegisterMockChannel(redis::detail::MockChannel* channel);

 private:
  friend class RedisClient;

  void Execute(const RedisCommand& command, Function<void(RedisObject&&)> cb,
               std::chrono::steady_clock::time_point timeout);

 private:
  rpc::internal::StreamCallGateHandle CreateCallGate(const Endpoint& endpoint);

 private:
  struct Impl;

  Options options_;
  std::unique_ptr<Impl> impl_;
};

}  // namespace flare

#endif  // FLARE_NET_REDIS_REDIS_CHANNEL_H_
