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

#include "flare/fiber/detail/waitable.h"

#include <array>
#include <utility>
#include <vector>

#include "flare/base/delayed_init.h"
#include "flare/base/logging.h"
#include "flare/base/ref_ptr.h"
#include "flare/fiber/detail/fiber_entity.h"
#include "flare/fiber/detail/scheduling_group.h"

namespace flare::fiber::detail {

namespace {

// Utility for waking up a fiber sleeping on a `Waitable` asynchronously.
class AsyncWaker {
 public:
  // Initialize an `AsyncWaker`.
  AsyncWaker(SchedulingGroup* sg, FiberEntity* self, WaitBlock* wb)
      : sg_(sg), self_(self), wb_(wb) {}

  // The destructor does some sanity checks.
  ~AsyncWaker() { FLARE_CHECK_EQ(timer_, 0, "Have you called `Cleanup()`?"); }

  // Set a timer to awake `self` once `expires_at` is reached.
  void SetTimer(std::chrono::steady_clock::time_point expires_at) {
    wait_cb_ = MakeRefCounted<WaitCb>();
    wait_cb_->waiter = self_;

    // This callback wakes us up if we times out.
    auto timer_cb = [wait_cb = wait_cb_ /* ref-counted */, wb = wb_](auto) {
      std::scoped_lock lk(wait_cb->lock);
      if (!wait_cb->awake) {  // It's (possibly) timed out.
        // We're holding the lock, and `wait_cb->awake` has not been set yet, so
        // `Cleanup()` cannot possibly finished yet. Therefore, we can be sure
        // `wb` is still alive.
        if (wb->satisfied.exchange(true, std::memory_order_relaxed)) {
          // Someone else satisfied the wait earlier.
          return;
        }
        wait_cb->waiter->scheduling_group->ReadyFiber(
            wait_cb->waiter, std::unique_lock(wait_cb->waiter->scheduler_lock));
      }
    };

    // Set the timeout timer.
    timer_ = sg_->CreateTimer(expires_at, timer_cb);
    sg_->EnableTimer(timer_);
  }

  // Prevent the timer set by this class from waking up `self` again.
  void Cleanup() {
    // If `timer_cb` has returned, nothing special; if `timer_cb` has never
    // started, nothing special. But if `timer_cb` is running, we need to
    // prevent it from `ReadyFiber` us again (when we immediately sleep on
    // another unrelated thing.).
    sg_->RemoveTimer(std::exchange(timer_, 0));
    {
      // Here is the trick.
      //
      // We're running now, therefore our `WaitBlock::satisfied` has been set.
      // Our `timer_cb` will check the flag, and bail out without waking us
      // again.
      std::scoped_lock _(wait_cb_->lock);
      wait_cb_->awake = true;
    }  // `wait_cb_->awake` has been set, so other fields of us won't be touched
       // by `timer_cb`. we're safe to destruct from now on.
  }

 private:
  // Ref counted as it's used both by us and an asynchronous timer.
  struct WaitCb : RefCounted<WaitCb> {
    Spinlock lock;
    FiberEntity* waiter;
    bool awake = false;
  };

  SchedulingGroup* sg_;
  FiberEntity* self_;
  WaitBlock* wb_;
  RefPtr<WaitCb> wait_cb_;
  std::uint64_t timer_ = 0;
};

}  // namespace

// Implementation of `Waitable` goes below.

bool Waitable::AddWaiter(WaitBlock* waiter) {
  std::scoped_lock _(lock_);

  FLARE_CHECK(waiter->waiter);
  if (persistent_awakened_) {
    return false;
  }
  waiters_.push_back(waiter);
  return true;
}

bool Waitable::TryRemoveWaiter(WaitBlock* waiter) {
  std::scoped_lock _(lock_);
  return waiters_.erase(waiter);
}

FiberEntity* Waitable::WakeOne() {
  std::scoped_lock _(lock_);
  while (true) {
    auto waiter = waiters_.pop_front();
    if (!waiter) {
      return nullptr;
    }
    // Memory order is guaranteed by `lock_`.
    if (waiter->satisfied.exchange(true, std::memory_order_relaxed)) {
      continue;  // It's awakened by someone else.
    }
    return waiter->waiter;
  }
}

std::vector<FiberEntity*> Waitable::SetPersistentAwakened() {
  std::scoped_lock _(lock_);
  persistent_awakened_ = true;

  std::vector<FiberEntity*> wbs;
  while (auto ptr = waiters_.pop_front()) {
    // Same as `WakeOne`.
    if (ptr->satisfied.exchange(true, std::memory_order_relaxed)) {
      continue;
    }
    wbs.push_back(ptr->waiter);
  }
  return wbs;
}

void Waitable::ResetAwakened() {
  std::scoped_lock _(lock_);
  persistent_awakened_ = false;
}

WaitableTimer::WaitableTimer(std::chrono::steady_clock::time_point expires_at)
    : sg_(SchedulingGroup::Current()),
      // Does it sense if we use object pool here?
      impl_(MakeRefCounted<WaitableRefCounted>()) {
  // We must not set the timer before `impl_` is initialized.
  timer_id_ = sg_->CreateTimer(
      expires_at, [ref = impl_](auto) { OnTimerExpired(std::move(ref)); });
  sg_->EnableTimer(timer_id_);
}

WaitableTimer::~WaitableTimer() { sg_->RemoveTimer(timer_id_); }

void WaitableTimer::wait() {
  FLARE_DCHECK(IsFiberContextPresent());

  auto current = GetCurrentFiberEntity();
  // `WaitBlock.chain` is initialized by `DoublyLinkedListEntry` itself.
  WaitBlock wb = {.waiter = current};
  std::unique_lock lk(current->scheduler_lock);
  if (impl_->AddWaiter(&wb)) {
    current->scheduling_group->Halt(current, std::move(lk));
    // We'll be awakened by `OnTimerExpired()`.
  } else {
    // The timer has fired before, return immediately then.
  }
}

void WaitableTimer::OnTimerExpired(RefPtr<WaitableRefCounted> ref) {
  auto fibers = ref->SetPersistentAwakened();
  for (auto f : fibers) {
    f->scheduling_group->ReadyFiber(f, std::unique_lock(f->scheduler_lock));
  }
}

// Implementation of `Mutex` goes below.

void Mutex::unlock() {
  FLARE_DCHECK(IsFiberContextPresent());

  if (auto was = count_.fetch_sub(1, std::memory_order_release); was == 1) {
    // Lucky day, no one is waiting on the mutex.
    //
    // Nothing to do.
  } else {
    FLARE_CHECK_GT(was, 1);

    // We need this lock so as to see a consistent state between `count_` and
    // `impl_` ('s internal wait queue).
    std::unique_lock splk(slow_path_lock_);
    auto fiber = impl_.WakeOne();
    FLARE_CHECK(fiber);  // Otherwise `was` must be 1 (as there's no waiter).
    splk.unlock();
    fiber->scheduling_group->ReadyFiber(
        fiber, std::unique_lock(fiber->scheduler_lock));
  }
}

void Mutex::LockSlow() {
  FLARE_DCHECK(IsFiberContextPresent());

  if (try_lock()) {
    return;  // Your lucky day.
  }

  // It's locked, take the slow path.
  std::unique_lock splk(slow_path_lock_);

  // Tell the owner that we're waiting for the lock.
  if (count_.fetch_add(1, std::memory_order_acquire) == 0) {
    // The owner released the lock before we incremented `count_`.
    //
    // We're still kind of lucky.
    return;
  }

  // Bad luck then. First we add us to the wait chain.
  auto current = GetCurrentFiberEntity();
  std::unique_lock slk(current->scheduler_lock);
  WaitBlock wb = {.waiter = current};
  FLARE_CHECK(impl_.AddWaiter(&wb));  // This can't fail as we never call
                                      // `SetPersistentAwakened()`.

  // Now the slow path lock can be unlocked.
  //
  // Indeed it's possible that we're awakened even before we call `Halt()`,
  // but this issue is already addressed by `scheduler_lock` (which we're
  // holding).
  splk.unlock();

  // Wait until we're woken by `unlock()`.
  //
  // Given that `scheduler_lock` is held by us, anyone else who concurrently
  // tries to wake us up is blocking on it until `Halt()` has completed.
  // Hence no race here.
  current->scheduling_group->Halt(current, std::move(slk));

  // Lock's owner has awakened us up, the lock is in our hand then.
  FLARE_DCHECK(!impl_.TryRemoveWaiter(&wb));
  return;
}

// Implementation of `ConditionVariable` goes below.

void ConditionVariable::wait(std::unique_lock<Mutex>& lock) {
  FLARE_DCHECK(IsFiberContextPresent());
  FLARE_DCHECK(lock.owns_lock());

  wait_until(lock, std::chrono::steady_clock::time_point::max());
}

bool ConditionVariable::wait_until(
    std::unique_lock<Mutex>& lock,
    std::chrono::steady_clock::time_point expires_at) {
  FLARE_DCHECK(IsFiberContextPresent());

  auto current = GetCurrentFiberEntity();
  auto sg = current->scheduling_group;
  bool use_timeout = expires_at != std::chrono::steady_clock::time_point::max();
  DelayedInit<AsyncWaker> awaker;

  // Add us to the wait queue.
  std::unique_lock slk(current->scheduler_lock);
  WaitBlock wb = {.waiter = current};
  FLARE_CHECK(impl_.AddWaiter(&wb));
  if (use_timeout) {  // Set a timeout if needed.
    awaker.Init(sg, current, &wb);
    awaker->SetTimer(expires_at);
  }

  // Release user's lock.
  lock.unlock();

  // Block until being waken up by either `notify_xxx` or the timer.
  sg->Halt(current, std::move(slk));  // `slk` is released by `Halt()`.

  // Try remove us from the wait chain. This operation will fail if we're
  // awakened by `notify_xxx()`.
  auto timeout = impl_.TryRemoveWaiter(&wb);

  if (awaker) {
    // Stop the timer we've set.
    awaker->Cleanup();
  }

  // Grab the lock again and return.
  lock.lock();
  return !timeout;
}

void ConditionVariable::notify_one() noexcept {
  FLARE_DCHECK(IsFiberContextPresent());

  auto fiber = impl_.WakeOne();
  if (!fiber) {
    return;
  }
  fiber->scheduling_group->ReadyFiber(fiber,
                                      std::unique_lock(fiber->scheduler_lock));
}

void ConditionVariable::notify_all() noexcept {
  FLARE_DCHECK(IsFiberContextPresent());

  // We cannot keep calling `notify_one` here. If a waiter immediately goes to
  // sleep again after we wake up it, it's possible that we wake it again when
  // we try to drain the wait chain.
  //
  // So we remove all waiters first, and schedule them then.
  std::array<FiberEntity*, 64> fibers_quick;
  std::size_t array_usage = 0;  // TODO(luobogao): We need a `FixedVector` in
                                // general.

  // We don't want to touch this in most cases.
  //
  // Given that `std::vector::vector()` is not allowed to throw, I do believe it
  // won't allocate memory on construction.
  std::vector<FiberEntity*> fibers_slow;

  while (true) {
    auto fiber = impl_.WakeOne();
    if (!fiber) {
      break;
    }
    if (FLARE_LIKELY(array_usage < std::size(fibers_quick))) {
      fibers_quick[array_usage++] = fiber;
    } else {
      fibers_slow.push_back(fiber);
    }
  }

  // Schedule the waiters.
  for (std::size_t index = 0; index != array_usage; ++index) {
    auto&& e = fibers_quick[index];
    e->scheduling_group->ReadyFiber(e, std::unique_lock(e->scheduler_lock));
  }
  for (auto&& e : fibers_slow) {
    e->scheduling_group->ReadyFiber(e, std::unique_lock(e->scheduler_lock));
  }
}

// Implementation of `ExitBarrier` goes below.

ExitBarrier::ExitBarrier() : count_(1) {}

std::unique_lock<Mutex> ExitBarrier::GrabLock() {
  FLARE_DCHECK(IsFiberContextPresent());
  return std::unique_lock(lock_);
}

void ExitBarrier::UnsafeCountDown(std::unique_lock<Mutex> lk) {
  FLARE_DCHECK(IsFiberContextPresent());
  FLARE_CHECK(lk.owns_lock() && lk.mutex() == &lock_);

  // tsan reports a data race if we unlock the lock before notifying the
  // waiters. Although I think it's a false positive, keep the lock before
  // notifying doesn't seem to hurt performance much.
  //
  // lk.unlock();
  FLARE_CHECK_GT(count_, 0);
  if (!--count_) {
    cv_.notify_all();
  }
}

void ExitBarrier::Wait() {
  FLARE_DCHECK(IsFiberContextPresent());

  std::unique_lock lk(lock_);
  return cv_.wait(lk, [this] { return count_ == 0; });
}

// Implementation of `Event` goes below.

void Event::Wait() {
  FLARE_DCHECK(IsFiberContextPresent());

  auto current = GetCurrentFiberEntity();
  WaitBlock wb = {.waiter = current};
  std::unique_lock lk(current->scheduler_lock);
  if (impl_.AddWaiter(&wb)) {
    current->scheduling_group->Halt(current, std::move(lk));
  } else {
    // The event is set already, return immediately.
  }
}

void Event::Set() {
  // `IsFiberContextPresent()` is not checked. This method is explicitly allowed
  // to be called out of fiber context.
  std::vector<FiberEntity*> fibers = impl_.SetPersistentAwakened();

  // Fiber wake-up must be delayed until we're done with `impl_`, otherwise
  // `impl_` can be destroyed after its emptied but before we touch it again.
  for (auto&& f : fibers) {
    f->scheduling_group->ReadyFiber(f, std::unique_lock(f->scheduler_lock));
  }
}

OneshotTimedEvent::OneshotTimedEvent(
    std::chrono::steady_clock::time_point expires_at)
    : sg_(SchedulingGroup::Current()), impl_(MakeRefCounted<Impl>()) {
  timer_id_ = sg_->CreateTimer(
      expires_at, [ref = impl_](auto) { OnTimerExpired(std::move(ref)); });
  sg_->EnableTimer(timer_id_);
}

OneshotTimedEvent::~OneshotTimedEvent() { sg_->RemoveTimer(timer_id_); }

void OneshotTimedEvent::Wait() { impl_->event.Wait(); }

void OneshotTimedEvent::Set() { impl_->IdempotentSet(); }

void OneshotTimedEvent::Impl::IdempotentSet() {
  if (!event_set_guard.exchange(true, std::memory_order_relaxed)) {
    event.Set();
  }
}

void OneshotTimedEvent::OnTimerExpired(RefPtr<Impl> ref) {
  ref->IdempotentSet();
}

}  // namespace flare::fiber::detail
