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

#ifndef FLARE_FIBER_DETAIL_WAITABLE_H_
#define FLARE_FIBER_DETAIL_WAITABLE_H_

#include <chrono>
#include <limits>
#include <mutex>
#include <utility>
#include <vector>

#include "flare/base/chrono.h"
#include "flare/base/internal/doubly_linked_list.h"
#include "flare/base/likely.h"
#include "flare/base/logging.h"
#include "flare/base/object_pool/ref_counted.h"
#include "flare/base/ref_ptr.h"
#include "flare/base/thread/spinlock.h"
#include "flare/fiber/detail/fiber_entity.h"

namespace flare::fiber::detail {

struct FiberEntity;
class SchedulingGroup;

// For waiting on an object, one or more `Waiter` structure should be allocated
// on waiter's stack for chaining into `Waitable`'s internal list.
//
// If the waiter is waiting on multiple `Waitable`s and has waken up by one of
// them, it's the waiter's responsibility to remove itself from the remaining
// `Waitable`s before continue running (so as not to induce use-after-free.).
struct WaitBlock {
  FiberEntity* waiter = nullptr;  // This initialization will be optimized away.
  flare::internal::DoublyLinkedListEntry chain;
  std::atomic<bool> satisfied = false;
};

// Basic class for implementing waitable classes.
//
// Do NOT use this class directly, it's meant to be used as a building block.
//
// Thread-safe.
class Waitable {
 public:
  Waitable() = default;
  ~Waitable() { FLARE_DCHECK(waiters_.empty()); }

  // Add a waiter to the tail.
  //
  // Returns `true` if the waiter is added to the wait chain, returns
  // `false` if the wait is immediately satisfied.
  //
  // To prevent wake-up loss, `FiberEntity::scheduler_lock` must be held by the
  // caller. (Otherwise before you take the lock, the fiber could have been
  // concurrently waken up, which is lost, by someone else.)
  bool AddWaiter(WaitBlock* waiter);

  // Remove a waiter.
  //
  // Returns false if the waiter is not linked.
  bool TryRemoveWaiter(WaitBlock* waiter);

  // Popup one waiter and schedule it.
  //
  // Returns `nullptr` if there's no waiter.
  FiberEntity* WakeOne();

  // Set this `Waitable` as "persistently" awakened. After this call, all
  // further calls to `AddWaiter` will fail.
  //
  // Pending waiters, if any, are returned.
  //
  // Be careful if you want to touch `Waitable` after calling this method. If
  // someone else failed `AddWaiter`, it would believe the wait was satisfied
  // immediately and could have freed this `Waitable` before you touch it again.
  //
  // Normally you should call `WakeAll` after calling this method.
  std::vector<FiberEntity*> SetPersistentAwakened();

  // Undo `SetPersistentAwakened()`.
  void ResetAwakened();

  // Noncopyable, nonmovable.
  Waitable(const Waitable&) = delete;
  Waitable& operator=(const Waitable&) = delete;

 private:
  Spinlock lock_;
  bool persistent_awakened_ = false;
  flare::internal::DoublyLinkedList<WaitBlock, &WaitBlock::chain> waiters_;
};

// "Waitable" timer. This `Waitable` signals all its waiters once the given time
// point is reached.
class WaitableTimer {
 public:
  explicit WaitableTimer(std::chrono::steady_clock::time_point expires_at);
  ~WaitableTimer();

  // Wait until the given time point is reached.
  void wait();

 private:
  // Reference counted `Waitable.
  struct WaitableRefCounted : Waitable, RefCounted<WaitableRefCounted> {};

  // This callback is implemented as a static method since the `WaitableTimer`
  // object can be destroyed (especially if the timer is allocated on waiter's
  // stack) before this method returns.
  static void OnTimerExpired(RefPtr<WaitableRefCounted> ref);

 private:
  SchedulingGroup* sg_;
  std::uint64_t timer_id_;

  // Protects `impl_`.
  Spinlock lock_;

  // We need to make this waitable ref-counted, otherwise if:
  //
  // - The user exits its fiber after awakened but before we finished with
  //   `Waitable`, and
  //
  // - The `Waitable` is allocated from user's stack.
  //
  // We'll be in trouble.
  RefPtr<WaitableRefCounted> impl_;
};

// Mutex for fiber.
class Mutex {
 public:
  bool try_lock() {
    FLARE_DCHECK(IsFiberContextPresent());

    std::uint32_t expected = 0;
    return count_.compare_exchange_strong(expected, 1,
                                          std::memory_order_acquire);
  }

  void lock() {
    FLARE_DCHECK(IsFiberContextPresent());

    if (FLARE_LIKELY(try_lock())) {
      return;
    }
    LockSlow();
  }

  void unlock();

 private:
  void LockSlow();

 private:
  Waitable impl_;

  // Synchronizes between slow path of `lock()` and `unlock()`.
  Spinlock slow_path_lock_;

  // Number of waiters (plus the owner). Hopefully `std::uint32_t` is large
  // enough.
  std::atomic<std::uint32_t> count_{0};
};

// Condition variable for fiber.
class ConditionVariable {
 public:
  void wait(std::unique_lock<Mutex>& lock);

  template <class F>
  void wait(std::unique_lock<Mutex>& lock, F&& pred) {
    FLARE_DCHECK(IsFiberContextPresent());

    while (!std::forward<F>(pred)()) {
      wait(lock);
    }
    FLARE_DCHECK(lock.owns_lock());
  }

  // You can always assume this method returns as a result of `notify_xxx` even
  // if it can actually results from timing out. This is conformant behavior --
  // it's just a spurious wake up in latter case.
  //
  // Returns `false` on timeout.
  bool wait_until(std::unique_lock<Mutex>& lock,
                  std::chrono::steady_clock::time_point expires_at);

  template <class F>
  bool wait_until(std::unique_lock<Mutex>& lk,
                  std::chrono::steady_clock::time_point timeout, F&& pred) {
    FLARE_DCHECK(IsFiberContextPresent());

    while (!std::forward<F>(pred)()) {
      wait_until(lk, timeout);
      if (ReadSteadyClock() >= timeout) {
        return std::forward<F>(pred)();
      }
    }
    FLARE_DCHECK(lk.owns_lock());
    return true;
  }

  void notify_one() noexcept;
  void notify_all() noexcept;

 private:
  Waitable impl_;
};

// ExitBarrier.
//
// This is effectively a `Latch` to implement `Fiber::join()`. However, unlike
// `Latch`, we cannot afford to block (which can, in case of `Latch`, since
// `count_down()` internally grabs lock) in waking up fibers (in master fiber.).
//
// Therefore we implement this class by separating grabbing the lock and wake up
// waiters, so that the user can grab the lock in advance, avoiding block in
// master fiber.
class ExitBarrier : public object_pool::RefCounted<ExitBarrier> {
 public:
  ExitBarrier();

  // Grab the lock required by `UnsafeCountDown()` in advance.
  std::unique_lock<Mutex> GrabLock();

  // Count down the barrier's internal counter and wake up waiters.
  void UnsafeCountDown(std::unique_lock<Mutex> lk);

  // Won't block.
  void Wait();

  void Reset() { count_ = 1; }

 private:
  Mutex lock_;
  std::size_t count_;
  ConditionVariable cv_;
};

// Emulates Event in Win32 API.
//
// For internal use only. Users should stick with `Mutex` + `ConditionVariable`
// instead.
class Event {
 public:
  // Wait until `Set()` is called. If `Set()` is called before `Wait()`, this
  // method returns immediately.
  void Wait();

  // Wake up fibers blocking on `Wait()`. All subsequent calls to `Wait()` will
  // return immediately.
  //
  // It's explicitly allowed to call this method outside of fiber context.
  void Set();

 private:
  Waitable impl_;
};

// This event type supports timeout, to some extent.
//
// For internal use only. Users should stick with `Mutex` + `ConditionVariable`
// instead.
class OneshotTimedEvent {
 public:
  // The event is automatically set when `expires_at` is reached (and `Set` has
  // not been called).
  //
  // This type may only be instantiated in fiber context.
  explicit OneshotTimedEvent(std::chrono::steady_clock::time_point expires_at);
  ~OneshotTimedEvent();

  // Wait until `Set()` has been called or timeout has expired.
  void Wait();

  // Wake up any fibers blocking on `Wait()` (if the timeout has not expired
  // yet).
  //
  // It's explicitly allowed to call this method outside of fiber context.
  void Set();

 private:
  struct Impl : RefCounted<Impl> {
    std::atomic<bool> event_set_guard{false};
    Event event;

    // Sets `event`. Calling this method multiple times is explicitly allowed.
    void IdempotentSet();
  };

  // Implemented as static method for the same reason as `WaitableTimer`.
  static void OnTimerExpired(RefPtr<Impl> ref);

 private:
  SchedulingGroup* sg_;
  std::uint64_t timer_id_;

  RefPtr<Impl> impl_;
};

}  // namespace flare::fiber::detail

namespace flare {

template <>
struct PoolTraits<fiber::detail::ExitBarrier> {
  static constexpr auto kType = PoolType::MemoryNodeShared;
  static constexpr auto kLowWaterMark = 32768;
  static constexpr auto kHighWaterMark =
      std::numeric_limits<std::size_t>::max();
  static constexpr auto kMaxIdle = std::chrono::seconds(10);
  static constexpr auto kMinimumThreadCacheSize = 8192;
  static constexpr auto kTransferBatchSize = 1024;

  static void OnGet(fiber::detail::ExitBarrier* e) { e->Reset(); }
};

}  // namespace flare

#endif  // FLARE_FIBER_DETAIL_WAITABLE_H_
