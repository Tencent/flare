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

#include "flare/fiber/detail/fiber_desc.h"

#include <chrono>
#include <cstddef>
#include <limits>

#include "flare/base/object_pool.h"
#include "flare/fiber/detail/waitable.h"

namespace flare {

template <>
struct PoolTraits<fiber::detail::FiberDesc> {
  static constexpr auto kType = PoolType::MemoryNodeShared;

  // Chosen arbitrarily. TODO(luobogao): Fine tuning.
  static constexpr auto kLowWaterMark = 16384;
  static constexpr auto kHighWaterMark =
      std::numeric_limits<std::size_t>::max();
  static constexpr auto kMaxIdle = std::chrono::seconds(10);
  static constexpr auto kMinimumThreadCacheSize = 4096;
  static constexpr auto kTransferBatchSize = 1024;

  static void OnPut(fiber::detail::FiberDesc* desc) {
    FLARE_CHECK(!desc->start_proc,
                "Unexpected: `FiberDesc` is destroyed without ever run.");
    FLARE_CHECK(
        !desc->exit_barrier,
        "Unexpected: `FiberDesc` is destroyed without being detached first.");
  }
};

}  // namespace flare

namespace flare::fiber::detail {

FiberDesc::FiberDesc() { SetRuntimeTypeTo<FiberDesc>(); }

FiberDesc* NewFiberDesc() noexcept {
  return object_pool::Get<FiberDesc>().Leak();
}

void DestroyFiberDesc(FiberDesc* desc) noexcept {
  object_pool::Put<FiberDesc>(desc);
}

}  // namespace flare::fiber::detail
