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

#ifndef FLARE_TESTING_REDIS_MOCK_H_
#define FLARE_TESTING_REDIS_MOCK_H_

#include <chrono>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "thirdparty/googletest/gmock/gmock.h"

#include "flare/base/function.h"
#include "flare/base/internal/lazy_init.h"
#include "flare/net/redis/mock_channel.h"
#include "flare/testing/detail/gmock_actions.h"

// Usage: `FLARE_EXPECT_REDIS_COMMAND({matcher})...`
//
// To manually provide values or errors, use `flare::testing::Return(...))` to
// return a `RedisObject in `WillXxx(...)`.
#define FLARE_EXPECT_REDIS_COMMAND(RequestMatcher)                           \
  ::testing::Mock::AllowLeak(::flare::internal::LazyInit<                    \
                             ::flare::testing::detail::MockRedisChannel>()); \
  EXPECT_CALL(*::flare::internal::LazyInit<                                  \
                  ::flare::testing::detail::MockRedisChannel>(),             \
              Execute(::testing::_ /* self, ignored */, RequestMatcher,      \
                      ::testing::_ /* cb, ignored */,                        \
                      ::testing::_ /* timeout, ignored */))

namespace flare::testing {

// Matches Redis request.
//
// Usage: FLARE_EXPECT_REDIS_COMMAND(RedisCommandEq(RedisCommand("GET", "key")))
auto RedisCommandEq(const flare::RedisCommand& expected);

// Matches opcode on Redis request.
//
// FLARE_EXPECT_REDIS_COMMAND(RedisCommandOpEq("GET"))
auto RedisCommandOpEq(const std::string& expected);

// Matches Redis request by calling user's callback.
//
// FLARE_EXPECT_REDIS_COMMAND(
//     RedisCommandUserMatch([&] (const RedisCommand& c) { return true; }))
auto RedisCommandUserMatch(Function<bool(const RedisCommand&)> cb);

// Parse `RedisCommand` to get operation being performed.
std::string GetRedisCommandOp(const RedisCommand& command);

}  // namespace flare::testing

////////////////////////////////////////
// Implementation goes below.         //
////////////////////////////////////////

namespace flare::testing {

namespace detail {

class MockRedisChannel : public flare::redis::detail::MockChannel {
 public:
  MOCK_METHOD4(Execute,
               void(const MockChannel* self, const RedisCommand* command,
                    Function<void(RedisObject&&)>* cb,
                    std::chrono::steady_clock::time_point timeout));

  using GMockActionArguments =
      std::tuple<const RedisCommand*, Function<void(RedisObject&&)>*,
                 std::chrono::steady_clock::time_point>;

  static void GMockActionReturn(const GMockActionArguments& arguments,
                                const RedisObject& values);
};

// Matches entire Redis command.
class RedisCommandEqImpl {
 public:
  explicit RedisCommandEqImpl(flare::RedisCommand expected);

  void DescribeTo(std::ostream* os) const;
  void DescribeNegationTo(::std::ostream* os) const;
  bool MatchAndExplain(const RedisCommand* command,
                       ::testing::MatchResultListener* result_listener) const;

 private:
  flare::RedisCommand expected_;
};

// Matches Redis command by operation.
class RedisCommandOpEqImpl {
 public:
  explicit RedisCommandOpEqImpl(std::string expected);

  void DescribeTo(std::ostream* os) const;
  void DescribeNegationTo(::std::ostream* os) const;
  bool MatchAndExplain(const RedisCommand* command,
                       ::testing::MatchResultListener* result_listener) const;

 private:
  std::string expected_;
};

// Matches Redis command using user specified callback.
class RedisCommandUserMatchImpl {
 public:
  explicit RedisCommandUserMatchImpl(
      Function<bool(const RedisCommand&)> matcher);

  void DescribeTo(std::ostream* os) const;
  void DescribeNegationTo(::std::ostream* os) const;
  bool MatchAndExplain(const RedisCommand* command,
                       ::testing::MatchResultListener* result_listener) const;

 private:
  // GMock does not get along well with move-only types, so we use
  // `shared_ptr<T>` to workaround that limitation.
  std::shared_ptr<Function<bool(const RedisCommand&)>> matcher_;
};

}  // namespace detail

inline auto RedisCommandEq(const RedisCommand& expected) {
  return ::testing::MakePolymorphicMatcher(
      detail::RedisCommandEqImpl(expected));
}

inline auto RedisCommandOpEq(const std::string& expected) {
  return ::testing::MakePolymorphicMatcher(
      detail::RedisCommandOpEqImpl(expected));
}

inline auto RedisCommandUserMatch(Function<bool(const RedisCommand&)> cb) {
  return ::testing::MakePolymorphicMatcher(
      detail::RedisCommandUserMatchImpl(std::move(cb)));
}

template <class T>
struct MockImplementationTraits;

template <>
struct MockImplementationTraits<redis::detail::MockChannel> {
  using type = detail::MockRedisChannel;
};

}  // namespace flare::testing

#endif  // FLARE_TESTING_REDIS_MOCK_H_
