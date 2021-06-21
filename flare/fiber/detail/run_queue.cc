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

#include "flare/fiber/detail/run_queue.h"

#include <cstdint>
#include <utility>

#include "flare/base/likely.h"
#include "flare/base/logging.h"
#include "flare/fiber/detail/assembly.h"

namespace flare::fiber::detail {

RunQueue::RunQueue(std::size_t capacity)
    : capacity_(capacity), mask_(capacity_ - 1) {
  FLARE_CHECK((capacity & (capacity - 1)) == 0,
              "Capacity must be a power of 2.");
  head_seq_.store(0, std::memory_order_relaxed);
  tail_seq_.store(0, std::memory_order_relaxed);
  nodes_ = std::make_unique<Node[]>(capacity_);
  for (std::size_t index = 0; index != capacity_; ++index) {
    nodes_[index].seq.store(index, std::memory_order_relaxed);  // `release`?
  }
}

RunQueue::~RunQueue() = default;

bool RunQueue::BatchPush(RunnableEntity** start, RunnableEntity** end,
                         bool instealable) {
  auto batch = end - start;
  while (true) {
    auto head_was = head_seq_.load(std::memory_order_relaxed);
    auto head = head_was + batch;
    auto hseq = nodes_[head & mask_].seq.load(std::memory_order_acquire);

    // Let's see if the last node we're trying to claim is not occupied.
    if (FLARE_LIKELY(hseq == head)) {
      // First check if the entire range is clean.
      for (int i = 0; i != batch; ++i) {
        auto&& n = nodes_[(head_was + i) & mask_];
        auto seq = n.seq.load(std::memory_order_acquire);
        if (FLARE_UNLIKELY(seq != head_was + i)) {
          if (seq + capacity_ == head_was + i + 1) {
            // This node hasn't been fully reset. Bail out.
            return false;
          }  // Fall-through otherwise.
        }
      }

      // Try claiming the entire range of [head_was, head).
      if (FLARE_LIKELY(head_seq_.compare_exchange_weak(
              head_was, head, std::memory_order_relaxed))) {
        // Fill them then.
        for (int i = 0; i != batch; ++i) {
          auto&& n = nodes_[(head_was + i) & mask_];
          FLARE_CHECK_EQ(n.seq.load(std::memory_order_relaxed), head_was + i);
          n.fiber = start[i];
          n.instealable.store(instealable, std::memory_order_relaxed);
          n.seq.store(head_was + i + 1, std::memory_order_release);
        }
        // TODO(luobogao): We can use `std::memory_order_relaxed` above when
        // storing into `n.seq`, and do a release fence for all of them here.
        return true;
      }
      // Fall-through otherwise.
    } else if (FLARE_UNLIKELY(hseq + capacity_ == head + 1)) {  // Overrun.
      // @sa: Comments in `PushSlow`.
      return false;
    } else {
      // We've been too late, retry.
    }  // Fall-through.
    Pause();
  }
}

RunnableEntity* RunQueue::Steal() {
  return PopIf([](const Node& node) {
    return !node.instealable.load(std::memory_order_relaxed);
  });
}

RunnableEntity* RunQueue::PopSlow() {
  return PopIf([](auto&&) { return true; });
}

bool RunQueue::PushSlow(RunnableEntity* e, bool instealable) {
  while (true) {
    auto head = head_seq_.load(std::memory_order_relaxed);
    auto&& n = nodes_[head & mask_];
    auto nseq = n.seq.load(std::memory_order_acquire);
    if (FLARE_LIKELY(nseq == head)) {
      if (FLARE_LIKELY(head_seq_.compare_exchange_weak(
              head, head + 1, std::memory_order_relaxed))) {
        n.fiber = e;
        n.instealable.store(instealable, std::memory_order_relaxed);
        n.seq.store(head + 1, std::memory_order_release);
        return true;
      }
      // Fall-through.
    } else if (FLARE_UNLIKELY(nseq + capacity_ == head + 1)) {  // Overrun.
      // To whoever is debugging this code:
      //
      // You can see "false positive" if you set a breakpoint or call `abort`
      // here.
      //
      // The reason is that the thread calling this method can be delayed
      // arbitrarily long after loading `head_seq_` and `n.seq`, but before
      // testing if `nseq + capacity_ == head + 1` holds. By the time the
      // expression is tested, it's possible that the queue has indeed been
      // emptied.
      //
      // Therefore, you can see this branch taken even if the queue is empty *at
      // some point* during this method's execution.
      //
      // This should be expected and handled by the caller (presumably you, as
      // you're debugging this method), though. The only thing a thread-safe
      // method can guarantee is that, at **some** point, but not every point,
      // during its call, its behavior conforms to what it's intended to do.
      //
      // Technically, this time point is called as the method's "linearization
      // point". And this method is lineralized at the instant when `n.seq` is
      // loaded.
      return false;
    } else {
      // We've been too late, retry.
    }  // Fall-through.
    Pause();
  }
}

bool RunQueue::UnsafeEmpty() const {
  return head_seq_.load(std::memory_order_relaxed) <=
         tail_seq_.load(std::memory_order_relaxed);
}

template <class F>
RunnableEntity* RunQueue::PopIf(F&& f) {
  while (true) {
    auto tail = tail_seq_.load(std::memory_order_relaxed);
    auto&& n = nodes_[tail & mask_];
    auto nseq = n.seq.load(std::memory_order_acquire);
    if (FLARE_LIKELY(nseq == tail + 1)) {
      // Test before claim ownership.
      if (!std::forward<F>(f)(n)) {
        return nullptr;
      }
      if (FLARE_LIKELY(tail_seq_.compare_exchange_weak(
              tail, tail + 1, std::memory_order_relaxed))) {
        (void)n.seq.load(std::memory_order_acquire);  // We need this fence.
        auto rc = n.fiber;
        n.seq.store(tail + capacity_, std::memory_order_release);
        return rc;
      }
    } else if (FLARE_UNLIKELY(nseq == tail ||               // Not filled yet
                              nseq + capacity_ == tail)) {  // Wrap around
      // Underrun.
      return nullptr;
    } else {
      // Fall-through.
    }
    Pause();
  }
}

}  // namespace flare::fiber::detail
