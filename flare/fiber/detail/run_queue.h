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

#ifndef FLARE_FIBER_DETAIL_RUN_QUEUE_H_
#define FLARE_FIBER_DETAIL_RUN_QUEUE_H_

#include <cstddef>

#include <atomic>
#include <memory>

#include "flare/base/align.h"
#include "flare/base/likely.h"

namespace flare::fiber::detail {

struct RunnableEntity;

// Thread-safe queue for storing runnable fibers.
class alignas(hardware_destructive_interference_size) RunQueue {
 public:
  // Initialize a queue whose capacity is `capacity`.
  //
  // `capacity` must be a power of 2.
  explicit RunQueue(std::size_t capacity);

  // Destroy the queue.
  ~RunQueue();

  // Push a fiber into the run queue.
  //
  // `instealable` should be `e.scheduling_group_local`. Internally we store
  // this value separately for `Steal` to use. This is required since `Steal`
  // cannot access `RunnableEntity` without claiming ownership of it. In the
  // meantime, once the ownership is claimed (and subsequently to find the
  // `RunnableEntity` cannot be stolen), it can't be revoked easily. So we treat
  // `RunnableEntity` as opaque, and avoid access `RunnableEntity` it at all.
  //
  // Returns `false` on overrun.
  bool Push(RunnableEntity* e, bool instealable) {
    auto head = head_seq_.load(std::memory_order_relaxed);
    auto&& n = nodes_[head & mask_];
    auto nseq = n.seq.load(std::memory_order_acquire);
    if (FLARE_LIKELY(nseq == head &&
                     head_seq_.compare_exchange_strong(
                         head, head + 1, std::memory_order_relaxed))) {
      n.fiber = e;
      n.instealable.store(instealable, std::memory_order_relaxed);
      n.seq.store(head + 1, std::memory_order_release);
      return true;
    }
    return PushSlow(e, instealable);
  }

  // Push fibers in batch into the run queue.
  //
  // Returns `false` on overrun.
  bool BatchPush(RunnableEntity** start, RunnableEntity** end,
                 bool instealable);

  // Pop a fiber from the run queue.
  //
  // Returns `nullptr` if the queue is empty.
  RunnableEntity* Pop() {
    auto tail = tail_seq_.load(std::memory_order_relaxed);
    auto&& n = nodes_[tail & mask_];
    auto nseq = n.seq.load(std::memory_order_acquire);
    if (nseq == tail + 1) {
      if (tail_seq_.compare_exchange_strong(tail, tail + 1,
                                            std::memory_order_relaxed)) {
        auto rc = n.fiber;
        n.seq.store(tail + capacity_, std::memory_order_release);
        return rc;
      }
    } else if (nseq == tail) {
      return nullptr;
    }
    return PopSlow();
  }

  // Steal a fiber from this run queue.
  //
  // If the first fibers in the queue was pushed with `instealable` set,
  // `nullptr` will be returned.
  //
  // Returns `nullptr` if the queue is empty.
  RunnableEntity* Steal();

  // Test if the queue is empty. The result might be inaccurate.
  bool UnsafeEmpty() const;

 private:
  struct alignas(hardware_destructive_interference_size) Node {
    RunnableEntity* fiber;
    std::atomic<bool> instealable;
    std::atomic<std::uint64_t> seq;
  };

  bool PushSlow(RunnableEntity* e, bool instealable);
  RunnableEntity* PopSlow();

  template <class F>
  RunnableEntity* PopIf(F&& f);

  std::size_t capacity_;
  std::size_t mask_;
  std::unique_ptr<Node[]> nodes_;
  alignas(hardware_destructive_interference_size)
      std::atomic<std::size_t> head_seq_;
  alignas(hardware_destructive_interference_size)
      std::atomic<std::size_t> tail_seq_;
};

}  // namespace flare::fiber::detail

#endif  // FLARE_FIBER_DETAIL_RUN_QUEUE_H_
