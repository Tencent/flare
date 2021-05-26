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

#ifndef FLARE_RPC_INTERNAL_FAST_LATCH_H_
#define FLARE_RPC_INTERNAL_FAST_LATCH_H_

#include <atomic>

#include "flare/fiber/condition_variable.h"
#include "flare/fiber/mutex.h"

namespace flare::rpc::detail {

// Behaves like a latch but optimized for non-blocking case.
class FastLatch {
 public:
  void wait() noexcept {
    if (auto v = left_.fetch_sub(1, std::memory_order_acquire);
        FLARE_LIKELY(v == 1)) {
      // Callback is run earlier. No heavy synchronization is required in this
      // case.
    } else {
      FLARE_CHECK_EQ(v, 2);
      WaitSlow();
    }
  }

  void count_down() noexcept {
    if (auto v = left_.fetch_sub(1, std::memory_order_release);
        FLARE_LIKELY(v == 2)) {
      // We're run first, no heavy synchronization is required as `Wait()`
      // will see 0 after decrementing `left_` again and won't block.
    } else {
      FLARE_CHECK_EQ(v, 1);
      NotifySlow();
    }
  }

 private:
  void WaitSlow() noexcept;
  void NotifySlow() noexcept;

  std::atomic<int> left_{2};
  fiber::Mutex lock_;
  fiber::ConditionVariable cv_;
  bool wake_up_{false};
};

}  // namespace flare::rpc::detail

#endif  // FLARE_RPC_INTERNAL_FAST_LATCH_H_
