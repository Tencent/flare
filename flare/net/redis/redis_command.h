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

#ifndef FLARE_NET_REDIS_REDIS_COMMAND_H_
#define FLARE_NET_REDIS_REDIS_COMMAND_H_

#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "flare/base/buffer.h"
#include "flare/base/string.h"

namespace flare {

// Represents a Redis command. This is usually a Redis request.
class RedisCommand {
 public:
  // Construct a Redis command.
  template <class... Ts>
  explicit RedisCommand(std::string_view op, const Ts&... args) {
    // https://redis.io/topics/protocol:
    //
    // > Clients send commands to a Redis server as a RESP Array of Bulk
    // > Strings.
    NoncontiguousBufferBuilder builder;

    // Array size.
    builder.Append("*", std::to_string(1 + sizeof...(args)), "\r\n");

    // Elements.
    AppendRedisCommandComponent(op, &builder);
    [[maybe_unused]] int dummy[] = {
        (AppendRedisCommandComponent(args, &builder), 0)...};

    // Save the request buffer.
    buffer_ = builder.DestructiveGet();
  }

  // Construct a Redis command from a vector of parameters.
  template <class StringLike>
  RedisCommand(std::string_view op, const std::vector<StringLike>& args) {
    NoncontiguousBufferBuilder builder;

    builder.Append("*", std::to_string(1 + args.size()), "\r\n");
    AppendRedisCommandComponent(op, &builder);
    for (auto&& e : args) {
      AppendRedisCommandComponent(e, &builder);
    }

    buffer_ = builder.DestructiveGet();
  }

  // Returns binary representation of this object.
  const NoncontiguousBuffer& GetBytes() const noexcept { return buffer_; }

 private:
  friend class RedisCommandBuilder;

  explicit RedisCommand(NoncontiguousBuffer bytes)
      : buffer_(std::move(bytes)) {}

  // Append a command component as Bulk String.
  static void AppendRedisCommandComponent(std::string_view component,
                                          NoncontiguousBufferBuilder* builder);

  static void AppendRedisCommandComponent(const NoncontiguousBuffer& component,
                                          NoncontiguousBufferBuilder* builder);

 private:
  NoncontiguousBuffer buffer_;
};

// This class helps you build complex Redis command.
class RedisCommandBuilder {
 public:
  RedisCommandBuilder();

  // Append a command component.
  void Append(std::string_view component);
  void Append(const NoncontiguousBuffer& component);

  // Build a Redis command. Once called, this builder may not be used.
  RedisCommand DestructiveGet();

 private:
  // Bytes reserved for the first line, "*(number of components)\r\n".
  //
  // Reserving 32 bytes should be far more than enough.
  inline static constexpr auto kReservedHdrSize = 32;

  char* reserved_header_;
  std::size_t components_ = 0;
  NoncontiguousBufferBuilder builder_;
};

}  // namespace flare

#endif  // FLARE_NET_REDIS_REDIS_COMMAND_H_
