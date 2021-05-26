// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "flare/net/redis/redis_command.h"

#include "flare/base/logging.h"

namespace flare {

void RedisCommand::AppendRedisCommandComponent(
    const std::string_view& component, NoncontiguousBufferBuilder* builder) {
  builder->Append("$", std::to_string(component.size()), "\r\n", component,
                  "\r\n");
}

void RedisCommand::AppendRedisCommandComponent(
    const NoncontiguousBuffer& component, NoncontiguousBufferBuilder* builder) {
  builder->Append("$", std::to_string(component.ByteSize()), "\r\n");
  builder->Append(component);
  builder->Append("\r\n");
}

RedisCommandBuilder::RedisCommandBuilder() {
  reserved_header_ = builder_.Reserve(kReservedHdrSize);
}

void RedisCommandBuilder::Append(const std::string_view& component) {
  ++components_;
  builder_.Append("$", std::to_string(component.size()), "\r\n", component,
                  "\r\n");
}

void RedisCommandBuilder::Append(const NoncontiguousBuffer& component) {
  ++components_;
  builder_.Append("$", std::to_string(component.ByteSize()), "\r\n");
  builder_.Append(component);
  builder_.Append("\r\n");
}

RedisCommand RedisCommandBuilder::DestructiveGet() {
  auto hdr = Format("*{}\r\n", components_);
  FLARE_CHECK_LE(hdr.size(), kReservedHdrSize,
                 "There are way too many components in the command.");

  auto bytes = builder_.DestructiveGet();
  // If we reserved more bytes than needed, drop them now.
  auto skip_size = kReservedHdrSize - hdr.size();
  bytes.Skip(skip_size);
  // Append header to the resulting bytes.
  memcpy(reserved_header_ + skip_size, hdr.data(), hdr.size());
  return RedisCommand(std::move(bytes));
}

}  // namespace flare
