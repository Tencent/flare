// Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef FLARE_RPC_INTERNAL_CORRELATION_ID_H_
#define FLARE_RPC_INTERNAL_CORRELATION_ID_H_

#include <cinttypes>
#include <limits>
#include <utility>

#include "flare/base/id_alloc.h"

namespace flare::rpc::internal {

struct U32CorrelationIdTraits {
  using Type = std::uint32_t;

  // I'm not sure if 0 should be avoided here. Our framework actually can handle
  // 0 correctly.
  static constexpr auto kMin = 1;
  static constexpr auto kMax = std::numeric_limits<std::uint32_t>::max();

  // This size can't be too large or we risk reusing still-in-use correlation
  // IDs.
  //
  // 128 means if we're delayed for about 40M allocations, we risk reusing IDs.
  // In case of 1M QPS, this means all threads except one are delayed for 40s,
  // but that's somewhat unlikely.
  //
  // Setting this value too small can greatly degrade performance.
  static constexpr auto kBatchSize = 128;
};

// Allocate new ID for RPC.
inline std::uint32_t NewRpcCorrelationId() {
  return id_alloc::Next<U32CorrelationIdTraits>();
}

// Allocate new ID for connection.
//
// Each connection has a dedicated ID. This is necessary to distinguish ID
// duplicates on different connections.
inline std::uint32_t NewConnectionCorrelationId() {
  // Given that connection establishment is inherently slow, I see few point in
  // optimizing this ID allocation.
  static std::atomic<std::uint32_t> next_id{};
  return next_id.fetch_add(1, std::memory_order_relaxed);
}

// Merge connection correlation ID and RPC correlation ID.
inline std::uint64_t MergeCorrelationId(std::uint32_t conn_cid,
                                        std::uint32_t rpc_cid) {
  return static_cast<std::uint64_t>(conn_cid) << 32 | rpc_cid;
}

// Split correlation ID merged by `MergeCorrelationId` to connection correlation
// ID and RPC correlation ID.
inline std::pair<std::uint32_t, std::uint32_t> SplitCorrelationId(
    std::uint64_t id) {
  return {id >> 32, id & 0xffff'ffff};
}

}  // namespace flare::rpc::internal

#endif  // FLARE_RPC_INTERNAL_CORRELATION_ID_H_
