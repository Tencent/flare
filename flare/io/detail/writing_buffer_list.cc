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

#include "flare/io/detail/writing_buffer_list.h"

#include <limits.h>

#include <algorithm>
#include <limits>
#include <utility>

#include "flare/base/chrono.h"
#include "flare/base/internal/annotation.h"
#include "flare/base/likely.h"
#include "flare/base/object_pool.h"
#include "flare/io/detail/eintr_safe.h"

using namespace std::literals;

namespace flare {

template <>
struct PoolTraits<io::detail::WritingBufferList::Node> {
  static constexpr auto kType = PoolType::MemoryNodeShared;
  static constexpr auto kLowWaterMark = 8192;
  static constexpr auto kHighWaterMark =
      std::numeric_limits<std::size_t>::max();
  static constexpr auto kMaxIdle = std::chrono::seconds(10);
  static constexpr auto kMinimumThreadCacheSize = 2048;
  static constexpr auto kTransferBatchSize = 2048;

  static void OnPut(io::detail::WritingBufferList::Node* p) {
    // This line serves two purposes:
    //
    // - We don't want to hold `p->buffer` after it's recycled, it simply a
    //   waste of resources.
    //
    // - This also prevent a nondeterministic behavior at program exit.
    //   Otherwise by the time `Node`'s pool is destroyed, it relies on
    //   `Buffer`'s pool being alive to destroy `p->buffer`, or we'll be in
    //   realm of UB.
    p->buffer.Clear();
  }
};

}  // namespace flare

namespace flare::io::detail {

WritingBufferList::WritingBufferList() {
  tail_.store(nullptr, std::memory_order_relaxed);
  // `head_` is not initialized here.
  //
  // Each time `tail_` is reset to `nullptr`, `head_` will be initialized by the
  // next call to `Append`.
}

WritingBufferList::~WritingBufferList() {
  // Update `head_` in case it's in inconsistent state (`FlushTo` can leave it
  // in such a state and leave it for `Append` to fix.).
  Append({}, {});

  // Free the list.
  auto current = head_.load(std::memory_order_acquire);
  while (current) {
    PooledPtr<Node> ptr(current);  // To be freed.
    current = current->next.load(std::memory_order_acquire);
  }
}

ssize_t WritingBufferList::FlushTo(AbstractStreamIo* io, std::size_t max_bytes,
                                   std::vector<std::uintptr_t>* flushed_ctxs,
                                   bool* emptied, bool* short_write) {
  // This array is likely to be large, so make it TLS to prevent StackOverflow
  // (tm).
  FLARE_INTERNAL_TLS_MODEL thread_local iovec iov[IOV_MAX];

  std::size_t nv = 0;
  std::size_t flushing = 0;

  // Yes since we're running concurrently with `Append`, we could miss some
  // newly-added buffers, and it does cause some (small) performance
  // degradation. However, it won't affect the whole algorithm's correctness.
  auto head = head_.load(std::memory_order_acquire);
  auto current = head;
  FLARE_CHECK(current);  // It can't be. `Append` should have already updated
                         // it.
  FLARE_CHECK(tail_.load(std::memory_order_relaxed), "The buffer is empty.");
  while (current) {
    for (auto iter = current->buffer.begin();
         iter != current->buffer.end() && nv != std::size(iov) &&
         flushing < max_bytes;
         ++iter) {
      auto&& e = iov[nv++];
      e.iov_base = const_cast<char*>(iter->data());
      e.iov_len = iter->size();  // For the last iov, we revise its size later.

      flushing += e.iov_len;
    }
    current = current->next.load(std::memory_order_acquire);
  }

  // See above. We might fill more bytes than allowed into `iov`, so we revise
  // the possible error here.
  if (FLARE_LIKELY(flushing > max_bytes)) {
    auto diff = flushing - max_bytes;
    iov[nv - 1].iov_len -= diff;
    flushing -= diff;
  }

  ssize_t rc = io->WriteV(iov, nv);
  if (rc <= 0) {
    return rc;  // Nothing is really flushed then.
  }
  FLARE_CHECK_LE(rc, flushing);

  // We did write something out. Remove those buffers and update the result
  // accordingly.
  auto flushed = static_cast<std::size_t>(rc);
  bool drained = false;

  // Rewind.
  //
  // We do not have to reload `head_`, it shouldn't have changed.
  current = head;
  while (current) {
    if (auto b = current->buffer.ByteSize(); b <= flushed) {
      // The entire buffer was written then.
      PooledPtr destroying(current);  // To be freed.

      flushed -= b;
      flushed_ctxs->push_back(current->ctx);
      if (auto next = current->next.load(std::memory_order_acquire); !next) {
        // We've likely drained the list.
        FLARE_CHECK_EQ(0, flushed);  // Or we have written out more than what we
                                     // have?
        // If nothing has changed, `tail_` still points to was-the-last node,
        // i.e., `current`.
        auto expected_tail = current;
        if (tail_.compare_exchange_strong(expected_tail, nullptr,
                                          std::memory_order_relaxed)) {
          // We successfully marked the list as empty.
          drained = true;
          // `head_` will be reset by next `Append`.
        } else {
          // Someone else is appending new node and has changed `tail_` to its
          // node. Here we wait for him until he finishes in updating `current`
          // (i.e., his `prev`)'s `next`. (@sa: `Append`.)
          Node* ptr;
          do {
            ptr = current->next.load(std::memory_order_acquire);
          } while (!ptr);
          // As `tail_` was never `nullptr` (we failed in setting it to
          // `nullptr` above), `Append` won't update `head_`. However, the newly
          // appended one is really the new head, so we update it here.
          head_.store(ptr, std::memory_order_release);
        }
        // In either case, we've finished rewinding, up to where we've written
        // out.
        break;
      } else {
        // Move to the next one.
        current = next;
      }
    } else {
      current->buffer.Skip(flushed);
      // We didn't drain the list, set `head_` to where we left off.
      head_.store(current, std::memory_order_release);
      break;
    }
  }

  *emptied = drained;
  *short_write = rc != flushing;
  return rc;
}

bool WritingBufferList::Append(NoncontiguousBuffer buffer, std::uintptr_t ctx) {
  auto node = object_pool::Get<Node>();

  node->next.store(nullptr, std::memory_order_relaxed);
  node->buffer = std::move(buffer);
  node->ctx = ctx;

  // By an atomic exchange between `tail_` and `node`, we atomically set `node`
  // as the new tail.
  auto prev = tail_.exchange(node.Get(), std::memory_order_acq_rel);
  if (!prev) {
    // If `tail_` was (before `exchange`) `nullptr`, the list was empty. In this
    // case we're the head of the list, so we'll update `head_` to reflect this.
    //
    // We'll also return `true` in this case to tell the caller about this fact.
    head_.store(node.Get(), std::memory_order_release);
  } else {
    // Otherwise there was a node (the old tail), so set us as its successor.
    FLARE_CHECK(!prev->next.load(std::memory_order_acquire));
    // Here is a time window between *`tail_` is set to `node`* and *`node` is
    // chained as the successor of the old `tail_`*.
    //
    // This inconsistency is mitigated by spinning in consumer (`FlushTo`) side
    // when it fails to `compare_exchange` `tail_` to `nullptr`. In this case,
    // the consumer side will spinning until it sees a non-`nullptr` `next` of
    // the old `tail_`.
    prev->next.store(node.Get(), std::memory_order_release);
  }
  (void)node.Leak();  // It will be freed on deque.

  return !prev;  // We changed `head_`.
}

}  // namespace flare::io::detail
