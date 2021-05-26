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

#ifndef FLARE_RPC_INTERNAL_SERVER_CONNECTION_HANDLER_H_
#define FLARE_RPC_INTERNAL_SERVER_CONNECTION_HANDLER_H_

#include <atomic>
#include <chrono>

#include "flare/base/chrono.h"
#include "flare/base/likely.h"
#include "flare/io/stream_connection.h"

namespace flare::rpc::detail {

// Base of connection handlers used by `Server`.
class ServerConnectionHandler : public StreamConnectionHandler {
  static constexpr auto kLastEventTimestampUpdateInterval =
      std::chrono::seconds(1);

 public:
  virtual void Stop() = 0;
  virtual void Join() = 0;

  std::chrono::steady_clock::time_point GetCoarseLastEventTimestamp()
      const noexcept {
    return std::chrono::steady_clock::time_point{
        next_update_.load(std::memory_order_relaxed) -
        kLastEventTimestampUpdateInterval};
  }

 protected:
  void ConsiderUpdateCoarseLastEventTimestamp() const noexcept {
    auto now = ReadCoarseSteadyClock().time_since_epoch();

    // We only write to `next_update_` after sufficient long period has passed
    // so as not to cause too many cache coherency traffic.
    if (FLARE_UNLIKELY(next_update_.load(std::memory_order_relaxed) < now)) {
      next_update_.store(now + kLastEventTimestampUpdateInterval,
                         std::memory_order_relaxed);
    }
  }

 private:
  mutable std::atomic<std::chrono::nanoseconds> next_update_{
      ReadCoarseSteadyClock().time_since_epoch() +
      kLastEventTimestampUpdateInterval};
};

}  // namespace flare::rpc::detail

#endif  // FLARE_RPC_INTERNAL_SERVER_CONNECTION_HANDLER_H_
