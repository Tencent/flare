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

#include "flare/testing/redis_mock.h"

#include <memory>
#include <string>
#include <utility>

#include "flare/base/internal/lazy_init.h"
#include "flare/base/logging.h"
#include "flare/init/on_init.h"
#include "flare/net/redis/reader.h"
#include "flare/net/redis/redis_channel.h"
#include "flare/net/redis/redis_command.h"

namespace flare::testing {

std::string GetRedisCommandOp(const RedisCommand& cmd) {
  RedisObject obj;
  auto buffer = cmd.GetBytes();
  FLARE_CHECK(redis::TryCutRedisObject(&buffer, &obj) > 0);
  FLARE_CHECK(obj.is<RedisArray>());
  auto&& elements = obj.as<RedisArray>();
  FLARE_CHECK_GE(elements->size(), 1);
  FLARE_CHECK((*elements)[0].is<RedisBytes>());
  return flare::FlattenSlow(*(*elements)[0].as<RedisBytes>());
}

}  // namespace flare::testing

namespace flare::testing::detail {

FLARE_ON_INIT(0 /* doesn't matter */, [] {
  RedisChannel::RegisterMockChannel(internal::LazyInit<MockRedisChannel>());
});

void MockRedisChannel::GMockActionReturn(const GMockActionArguments& arguments,
                                         const RedisObject& object) {
  RedisObject moving = object;
  (*std::get<1>(arguments))(std::move(moving));
}

RedisCommandOpEqImpl::RedisCommandOpEqImpl(std::string expected)
    : expected_(std::move(expected)) {}

void RedisCommandOpEqImpl::DescribeTo(std::ostream* os) const {
  (*os) << " opcode does not match ";
}

void RedisCommandOpEqImpl::DescribeNegationTo(::std::ostream* os) const {
  (*os) << " opcode matches ";
}

bool RedisCommandOpEqImpl::MatchAndExplain(
    const RedisCommand* command,
    ::testing::MatchResultListener* result_listener) const {
  return GetRedisCommandOp(*command) == expected_;
}

RedisCommandEqImpl::RedisCommandEqImpl(flare::RedisCommand expected)
    : expected_(std::move(expected)) {}

void RedisCommandEqImpl::DescribeTo(std::ostream* os) const {
  (*os) << " keys does not match ";
}

void RedisCommandEqImpl::DescribeNegationTo(::std::ostream* os) const {
  (*os) << " keys match ";
}

bool RedisCommandEqImpl::MatchAndExplain(
    const RedisCommand* command,
    ::testing::MatchResultListener* result_listener) const {
  auto expecting = FlattenSlow(expected_.GetBytes());
  auto given = FlattenSlow(command->GetBytes());
  if (expecting != given) {
    return false;
  }
  return true;
}

RedisCommandUserMatchImpl::RedisCommandUserMatchImpl(
    Function<bool(const RedisCommand&)> matcher)
    : matcher_(std::make_shared<Function<bool(const RedisCommand&)>>(
          std::move(matcher))) {}

void RedisCommandUserMatchImpl::DescribeTo(std::ostream* os) const {
  (*os) << " user's callback is not satisfied with ";
}

void RedisCommandUserMatchImpl::DescribeNegationTo(::std::ostream* os) const {
  (*os) << " user's callback is satisfied with ";
}

bool RedisCommandUserMatchImpl::MatchAndExplain(
    const RedisCommand* command,
    ::testing::MatchResultListener* result_listener) const {
  return (*matcher_)(*command);
}

}  // namespace flare::testing::detail
