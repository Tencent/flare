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

#include "flare/fiber/detail/scheduling_group.h"

#include <linux/futex.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>

#include <climits>
#include <memory>
#include <string>
#include <thread>

#include "gflags/gflags.h"

#include "flare/base/align.h"
#include "flare/base/deferred.h"
#include "flare/base/exposed_var.h"
#include "flare/base/internal/annotation.h"
#include "flare/base/internal/builtin_monitoring.h"
#include "flare/base/thread/latch.h"
#include "flare/base/object_pool.h"
#include "flare/base/random.h"
#include "flare/base/string.h"
#include "flare/base/tsc.h"
#include "flare/fiber/detail/assembly.h"
#include "flare/fiber/detail/fiber_entity.h"
#include "flare/fiber/detail/timer_worker.h"
#include "flare/fiber/detail/waitable.h"

using namespace std::literals;

// Note that unless the user do not enable guard page (in stack), there's always
// a limit on maximum alive (not only runnable) fibers in the process.
//
// @sa: `StackAllocator` for more details.
DEFINE_int32(flare_fiber_run_queue_size, 65536,
             "Maximum runnable fibers per scheduling group. This value must be "
             "a power of 2.");

namespace flare::fiber::detail {

flare::internal::ExposedMetricsInTsc ready_to_run_latency(
    "flare/fiber/latency/ready_to_run");
flare::internal::ExposedMetricsInTsc start_fibers_latency(
    "flare/fiber/latency/start_fibers");
flare::internal::ExposedMetricsInTsc wakeup_sleeping_worker_latency(
    "flare/fiber/latency/wakeup_sleeping_worker");
ExposedCounter<std::uint64_t> spinning_worker_wakeups(
    "flare/fiber/scheduling_group/spinning_worker_wakeups");
ExposedCounter<std::uint64_t> sleeping_worker_wakeups(
    "flare/fiber/scheduling_group/sleeping_worker_wakeups");
ExposedCounter<std::uint64_t> no_worker_available(
    "flare/fiber/scheduling_group/no_worker_available");

// If desired, user can report this timer to their monitoring system.
flare::internal::BuiltinMonitoredTimer ready_to_run_latency_monitoring(
    "flare_fiber_latency_ready_to_run", 1us);

namespace {

std::string WriteBitMask(std::uint64_t x) {
  std::string s(64, 0);
  for (int i = 0; i != 64; ++i) {
    s[i] = ((x & (1ULL << i)) ? '1' : '0');
  }
  return s;
}

}  // namespace

// This class guarantees no wake-up loss by keeping a "wake-up count". If a wake
// operation is made before a wait, the subsequent wait is immediately
// satisfied without actual going to sleep.
class alignas(hardware_destructive_interference_size)
    SchedulingGroup::WaitSlot {
 public:
  void Wake() noexcept {
    ScopedDeferred _([start = ReadTsc()] {
      wakeup_sleeping_worker_latency->Report(TscElapsed(start, ReadTsc()));
    });

    if (wakeup_count_.fetch_add(1, std::memory_order_relaxed) == 0) {
      FLARE_PCHECK(syscall(SYS_futex, &wakeup_count_, FUTEX_WAKE_PRIVATE, 1, 0,
                           0, 0) >= 0);
    }
    // If `Wait()` is called before this check fires, `wakeup_count_` can be 0.
    FLARE_CHECK_GE(wakeup_count_.load(std::memory_order_relaxed), 0);
  }

  void Wait() noexcept {
    if (wakeup_count_.fetch_sub(1, std::memory_order_relaxed) == 1) {
      do {
        // TODO(luobogao): I saw spurious wake up. But how can it happen? If
        // `wakeup_count_` is not zero by the time `futex` checks it, the only
        // values it can become is a positive one, which in this case is a
        // "real" wake up.
        //
        // We need further investigation here.
        auto rc =
            syscall(SYS_futex, &wakeup_count_, FUTEX_WAIT_PRIVATE, 0, 0, 0, 0);
        FLARE_PCHECK(rc == 0 || errno == EAGAIN);
      } while (wakeup_count_.load(std::memory_order_relaxed) == 0);
    }
    FLARE_CHECK_GT(wakeup_count_.load(std::memory_order_relaxed), 0);
  }

  void PersistentWake() noexcept {
    // Hopefully this is large enough.
    wakeup_count_.store(0x4000'0000, std::memory_order_relaxed);
    FLARE_PCHECK(syscall(SYS_futex, &wakeup_count_, FUTEX_WAKE_PRIVATE, INT_MAX,
                         0, 0, 0) >= 0);
  }

 private:
  // `futex` requires this.
  static_assert(sizeof(std::atomic<int>) == sizeof(int));

  std::atomic<int> wakeup_count_{1};
};

FLARE_INTERNAL_TLS_MODEL thread_local std::size_t
    SchedulingGroup::worker_index_ = kUninitializedWorkerIndex;

SchedulingGroup::SchedulingGroup(const std::vector<int>& affinity,
                                 std::size_t size)
    : group_size_(size),
      affinity_(affinity),
      run_queue_(FLAGS_flare_fiber_run_queue_size) {
  // We use bitmask (a `std::uint64_t`) to save workers' state. That puts an
  // upper limit on how many workers can be in a given scheduling group.
  FLARE_CHECK_LE(group_size_, 64,
                 "We only support up to 64 workers in each scheduling group. "
                 "Use more scheduling groups if you want more concurrency.");

  // Exposes states about this scheduling group.
  auto exposed_var_prefix =
      Format("flare/fiber/scheduling_group/{}/", fmt::ptr(this));

  spinning_workers_var_.Init(exposed_var_prefix + "spinning_workers", [this] {
    return WriteBitMask(spinning_workers_.load(std::memory_order_relaxed));
  });
  sleeping_workers_var_.Init(exposed_var_prefix + "sleeping_workers", [this] {
    return WriteBitMask(sleeping_workers_.load(std::memory_order_relaxed));
  });

  wait_slots_ = std::make_unique<WaitSlot[]>(group_size_);
}

SchedulingGroup::~SchedulingGroup() = default;

FiberEntity* SchedulingGroup::AcquireFiber() noexcept {
  if (auto rc = run_queue_.Pop()) {
    // Acquiring the lock here guarantees us anyone who is working on this fiber
    // (with the lock held) has done its job before we returning it to the
    // caller (worker).
    std::scoped_lock _(rc->scheduler_lock);

    FLARE_CHECK(rc->state == FiberState::Ready);
    rc->state = FiberState::Running;

    auto now = ReadTsc();
    ready_to_run_latency->Report(TscElapsed(rc->last_ready_tsc, now));
    ready_to_run_latency_monitoring.Report(
        DurationFromTsc(rc->last_ready_tsc, now));
    return rc;
  }
  return stopped_.load(std::memory_order_relaxed) ? kSchedulingGroupShuttingDown
                                                  : nullptr;
}

FiberEntity* SchedulingGroup::SpinningAcquireFiber() noexcept {
  // We don't want too many workers spinning, it wastes CPU cycles.
  static constexpr auto kMaximumSpinners = 2;

  FLARE_CHECK_NE(worker_index_, kUninitializedWorkerIndex);
  FLARE_CHECK_LT(worker_index_, group_size_);

  FiberEntity* fiber = nullptr;
  auto spinning = spinning_workers_.load(std::memory_order_relaxed);
  auto mask = 1ULL << worker_index_;
  bool need_spin = false;

  // Simply test `spinning` and try to spin may result in too many workers to
  // spin, as it there's a time windows between we test `spinning` and set our
  // bit in it.
  while (CountNonZeros(spinning) < kMaximumSpinners) {
    FLARE_DCHECK_EQ(spinning & mask, 0);
    if (spinning_workers_.compare_exchange_weak(spinning, spinning | mask,
                                                std::memory_order_relaxed)) {
      need_spin = true;
      break;
    }
  }

  if (need_spin) {
    static constexpr auto kMaximumCyclesToSpin = 10'000;
    // Wait for some time between touching `run_queue_` to reduce contention.
    static constexpr auto kCyclesBetweenRetry = 1000;
    auto start = ReadTsc(), end = start + kMaximumCyclesToSpin;

    ScopedDeferred _([&] {
      // Note that we can actually clear nothing, the same bit can be cleared by
      // `WakeOneSpinningWorker` simultaneously. This is okay though, as we'll
      // try `AcquireFiber()` when we leave anyway.
      spinning_workers_.fetch_and(~mask, std::memory_order_relaxed);
    });

    do {
      if (auto rc = AcquireFiber()) {
        fiber = rc;
        break;
      }
      auto next = start + kCyclesBetweenRetry;
      while (start < next) {
        if (pending_spinner_wakeup_.load(std::memory_order_relaxed) &&
            pending_spinner_wakeup_.exchange(false,
                                             std::memory_order_relaxed)) {
          // There's a pending wakeup, and it's us who is chosen to finish this
          // job.
          WakeUpOneDeepSleepingWorker();
        } else {
          Pause<16>();
        }
        start = ReadTsc();
      }
    } while (start < end &&
             (spinning_workers_.load(std::memory_order_relaxed) & mask));
  } else {
    // Otherwise there are already at least 2 workers spinning, don't waste CPU
    // cycles then.
    return nullptr;
  }

  if (fiber || ((fiber = AcquireFiber()))) {
    // Given that we successfully grabbed a fiber to run, we're likely under
    // load. So wake another worker to spin (if there are not enough spinners).
    //
    // We don't want to wake it here, though, as we've had something useful to
    // do (run `fiber)`, so we leave it for other spinners to wake as they have
    // nothing useful to do anyway.
    if (CountNonZeros(spinning_workers_.load(std::memory_order_relaxed)) <
        kMaximumSpinners) {
      pending_spinner_wakeup_.store(true, std::memory_order_relaxed);
    }
  }
  return fiber;
}

FiberEntity* SchedulingGroup::WaitForFiber() noexcept {
  FLARE_CHECK_NE(worker_index_, kUninitializedWorkerIndex);
  FLARE_CHECK_LT(worker_index_, group_size_);
  auto mask = 1ULL << worker_index_;

  while (true) {
    ScopedDeferred _([&] {
      // If we're woken up before we even sleep (i.e., we're "woken up" after we
      // set the bit in `sleeping_workers_` but before we actually called
      // `WaitSlot::Wait()`), this effectively clears nothing.
      sleeping_workers_.fetch_and(~mask, std::memory_order_relaxed);
    });
    FLARE_CHECK_EQ(
        sleeping_workers_.fetch_or(mask, std::memory_order_relaxed) & mask, 0);

    // We should test if the queue is indeed empty, otherwise if a new fiber is
    // put into the ready queue concurrently, and whoever makes the fiber ready
    // checked the sleeping mask before we updated it, we'll lose the fiber.
    if (auto f = AcquireFiber()) {
      // A new fiber is put into ready queue concurrently then.
      //
      // If our sleeping mask has already been cleared (by someone else), we
      // need to wake up yet another sleeping worker (otherwise it's a wakeup
      // miss.).
      //
      // Note that in this case the "deferred" clean up is not needed actually.
      // This is a rare case, though. TODO(luobogao): Optimize it away.
      if ((sleeping_workers_.fetch_and(~mask, std::memory_order_relaxed) &
           mask) == 0) {
        // Someone woken us up before we cleared the flag, wake up a new worker
        // for him.
        WakeUpOneWorker();
      }
      return f;
    }

    wait_slots_[worker_index_].Wait();

    // We only return non-`nullptr` here. If we return `nullptr` to the caller,
    // it'll go spinning immediately. Doing that will likely waste CPU cycles.
    if (auto f = AcquireFiber()) {
      return f;
    }  // Otherwise try again (and possibly sleep) until a fiber is ready.
  }
}

FiberEntity* SchedulingGroup::RemoteAcquireFiber() noexcept {
  if (auto rc = run_queue_.Steal()) {
    std::scoped_lock _(rc->scheduler_lock);

    FLARE_CHECK(rc->state == FiberState::Ready);
    rc->state = FiberState::Running;
    ready_to_run_latency->Report(TscElapsed(rc->last_ready_tsc, ReadTsc()));

    // It now belongs to the caller's scheduling group.
    rc->scheduling_group = Current();
    return rc;
  }
  return nullptr;
}

void SchedulingGroup::StartFibers(FiberEntity** start,
                                  FiberEntity** end) noexcept {
  if (FLARE_UNLIKELY(start == end)) {
    return;  // Why would you call this method then?
  }

  auto tsc = ReadTsc();
  ScopedDeferred _(
      [&] { start_fibers_latency->Report(TscElapsed(tsc, ReadTsc())); });

  for (auto iter = start; iter != end; ++iter) {
    (*iter)->state = FiberState::Ready;
    (*iter)->scheduling_group = this;
    (*iter)->last_ready_tsc = tsc;
  }
  if (FLARE_UNLIKELY(!run_queue_.BatchPush(start, end, false))) {
    auto since = ReadSteadyClock();

    while (!run_queue_.BatchPush(start, end, false)) {
      FLARE_LOG_WARNING_EVERY_SECOND(
          "Run queue overflow. Too many ready fibers to run. If you're still "
          "not overloaded, consider increasing `flare_fiber_run_queue_size`.");
      FLARE_LOG_FATAL_IF(ReadSteadyClock() - since > 5s,
                         "Failed to push fiber into ready queue after retrying "
                         "for 5s. Gave up.");
      std::this_thread::sleep_for(100us);
    }
  }
  // TODO(luobogao): Increment `no_worker_available` accordingly.
  WakeUpWorkers(end - start);
}

void SchedulingGroup::ReadyFiber(
    FiberEntity* fiber, std::unique_lock<Spinlock>&& scheduler_lock) noexcept {
  FLARE_DCHECK(!stopped_.load(std::memory_order_relaxed),
               "The scheduling group has been stopped.");
  FLARE_DCHECK_NE(fiber, GetMasterFiberEntity(),
                  "Master fiber should not be added to run queue.");

  fiber->state = FiberState::Ready;
  fiber->scheduling_group = this;
  fiber->last_ready_tsc = ReadTsc();
  if (scheduler_lock) {
    scheduler_lock.unlock();
  }

  // Push the fiber into run queue and (optionally) wake up a worker.
  if (FLARE_UNLIKELY(!run_queue_.Push(fiber, fiber->scheduling_group_local))) {
    auto since = ReadSteadyClock();

    while (!run_queue_.Push(fiber, fiber->scheduling_group_local)) {
      FLARE_LOG_WARNING_EVERY_SECOND(
          "Run queue overflow. Too many ready fibers to run. If you're still "
          "not overloaded, consider increasing `flare_fiber_run_queue_size`.");
      FLARE_LOG_FATAL_IF(ReadSteadyClock() - since > 5s,
                         "Failed to push fiber into ready queue after retrying "
                         "for 5s. Gave up.");
      std::this_thread::sleep_for(100us);
    }
  }
  if (FLARE_UNLIKELY(!WakeUpOneWorker())) {
    no_worker_available->Increment();
  }
}

void SchedulingGroup::Halt(
    FiberEntity* self, std::unique_lock<Spinlock>&& scheduler_lock) noexcept {
  FLARE_CHECK_EQ(self, GetCurrentFiberEntity(),
                 "`self` must be pointer to caller's `FiberEntity`.");
  FLARE_CHECK(
      scheduler_lock.owns_lock(),
      "Scheduler lock must be held by caller prior to call to this method.");
  FLARE_CHECK(
      self->state == FiberState::Running,
      "`Halt()` is only for running fiber's use. If you want to `ReadyFiber()` "
      "yourself and `Halt()`, what you really need is `Yield()`.");
  auto master = GetMasterFiberEntity();
  self->state = FiberState::Waiting;

  // We simply yield to master fiber for now.
  //
  // TODO(luobogao): We can directly yield to next ready fiber. This way we can
  // eliminate a context switch.
  //
  // Note that we need to hold `scheduler_lock` until we finished context swap.
  // Otherwise if we're in ready queue, we can be resume again even before we
  // stopped running. This will be disastrous.
  //
  // Do NOT pass `scheduler_lock` ('s pointer)` to cb. Instead, play with raw
  // lock.
  //
  // The reason is that, `std::unique_lock<...>::unlock` itself is not an atomic
  // operation (although `Spinlock` is).
  //
  // What this means is that, after unlocking the scheduler lock, and the fiber
  // starts to run again, `std::unique_lock<...>::owns_lock` does not
  // necessarily be updated in time (before the fiber checks it), which can lead
  // to subtle bugs.
  master->ResumeOn(
      [self_lock = scheduler_lock.release()]() { self_lock->unlock(); });

  // When we're back, we should be in the same fiber.
  FLARE_CHECK_EQ(self, GetCurrentFiberEntity());
}

void SchedulingGroup::Yield(FiberEntity* self) noexcept {
  // TODO(luobogao): We can directly yield to next ready fiber. This way we can
  // eliminate a context switch.
  auto master = GetMasterFiberEntity();

  // Master's state is not maintained in a coherency way..
  master->state = FiberState::Ready;
  SwitchTo(self, master);
}

void SchedulingGroup::SwitchTo(FiberEntity* self, FiberEntity* to) noexcept {
  FLARE_CHECK_EQ(self, GetCurrentFiberEntity());

  // We need scheduler lock here actually (at least to comfort TSan). But so
  // long as this check does not fiber, we're safe without the lock I think.
  FLARE_CHECK(to->state == FiberState::Ready,
              "Fiber `to` is not in ready state.");
  FLARE_CHECK_NE(self, to, "Switch to yourself results in U.B.");

  // TODO(luobogao): Ensure neither `self->scheduler_lock` nor
  // `to->scheduler_lock` is currrently held (by someone else).

  // We delay queuing `self` to run queue until `to` starts to run.
  //
  // It's possible that we first add `self` to run queue with its scheduler lock
  // locked, and unlock the lock when `to` runs. However, if `self` is grabbed
  // by some worker prior `to` starts to run, the worker will spin to wait for
  // `to`. This can be quite costly.
  to->ResumeOn([this, self]() {
    ReadyFiber(self, std::unique_lock(self->scheduler_lock));
  });

  // When we're back, we should be in the same fiber.
  FLARE_CHECK_EQ(self, GetCurrentFiberEntity());
}

void SchedulingGroup::EnterGroup(std::size_t index) {
  FLARE_CHECK(current_ == nullptr,
              "This pthread worker has already joined a scheduling group.");
  FLARE_CHECK(timer_worker_ != nullptr,
              "The timer worker is not available yet.");

  // Initialize TLSes as much as possible. Initializing them need an adequate
  // amount of stack space, and may not be done on system fiber.
  object_pool::internal::InitializeObjectPoolForCurrentThread();

  // Initialize thread-local timer queue for this worker.
  timer_worker_->InitializeLocalQueue(index);

  // Initialize scheduling-group information of this pthread.
  current_ = this;
  worker_index_ = index;

  // Initialize master fiber for this worker.
  SetUpMasterFiberEntity();
}

void SchedulingGroup::LeaveGroup() {
  FLARE_CHECK(current_ == this,
              "This pthread worker does not belong to this scheduling group.");
  current_ = nullptr;
  worker_index_ = kUninitializedWorkerIndex;
}

std::size_t SchedulingGroup::GroupSize() const noexcept { return group_size_; }

const std::vector<int>& SchedulingGroup::Affinity() const noexcept {
  return affinity_;
}

void SchedulingGroup::SetTimerWorker(TimerWorker* worker) noexcept {
  timer_worker_ = worker;
}

void SchedulingGroup::Stop() {
  stopped_.store(true, std::memory_order_relaxed);
  for (std::size_t index = 0; index != group_size_; ++index) {
    wait_slots_[index].PersistentWake();
  }
}

bool SchedulingGroup::WakeUpOneWorker() noexcept {
  return WakeUpOneSpinningWorker() || WakeUpOneDeepSleepingWorker();
}

bool SchedulingGroup::WakeUpOneSpinningWorker() noexcept {
  // FIXME: Is "relaxed" order sufficient here?
  while (auto spinning_mask =
             spinning_workers_.load(std::memory_order_relaxed)) {
    // Last fiber worker (LSB in `spinning_mask`) that is spinning.
    auto last_spinning = __builtin_ffsll(spinning_mask) - 1;
    auto claiming_mask = 1ULL << last_spinning;
    if (FLARE_LIKELY(spinning_workers_.fetch_and(~claiming_mask,
                                                 std::memory_order_relaxed) &
                     claiming_mask)) {
      // We cleared the `last_spinning` bit, no one else will try to dispatch
      // work to it.
      spinning_worker_wakeups->Add(1);
      return true;  // Fast path then.
    }
    Pause();
  }  // Keep trying until no one else is spinning.
  return false;
}

bool SchedulingGroup::WakeUpWorkers(std::size_t n) noexcept {
  if (FLARE_UNLIKELY(n == 0)) {
    return false;  // No worker is waken up.
  }
  if (FLARE_UNLIKELY(n == 1)) {
    return WakeUpOneWorker();
  }

  // As there are at most two spinners, and `n` is at least two, we can safely
  // claim all spinning workers.
  auto spinning_mask_was =
      spinning_workers_.exchange(0, std::memory_order_relaxed);
  auto woke = CountNonZeros(spinning_mask_was);
  sleeping_worker_wakeups->Add(woke);
  FLARE_CHECK_LE(woke, n);
  n -= woke;

  if (n >= group_size_) {
    // If there are more fibers than the group size, wake up all workers.
    auto sleeping_mask_was =
        sleeping_workers_.exchange(0, std::memory_order_relaxed);
    for (int i = 0; i != group_size_; ++i) {
      if (sleeping_mask_was & (1 << i)) {
        wait_slots_[i].Wake();
      }
      sleeping_worker_wakeups->Add(1);
    }
    return true;
  } else if (n >= 1) {
    while (auto sleeping_mask_was =
               sleeping_workers_.load(std::memory_order_relaxed)) {
      auto mask_to = sleeping_mask_was;
      // Wake up workers with lowest indices.
      if (CountNonZeros(sleeping_mask_was) <= n) {
        mask_to = 0;  // All workers will be woken up.
      } else {
        while (n--) {
          mask_to &= ~(1 << __builtin_ffsll(mask_to));
        }
      }

      // Try to claim the workers.
      if (FLARE_LIKELY(sleeping_workers_.compare_exchange_weak(
              sleeping_mask_was, mask_to, std::memory_order_relaxed))) {
        auto masked = sleeping_mask_was & ~mask_to;
        for (int i = 0; i != group_size_; ++i) {
          if (masked & (1 << i)) {
            wait_slots_[i].Wake();
          }
          sleeping_worker_wakeups->Add(1);
        }
        return true;
      }
      Pause();
    }
  } else {
    return true;
  }
  return false;
}

bool SchedulingGroup::WakeUpOneDeepSleepingWorker() noexcept {
  // We indeed have to wake someone that is in deep sleep then.
  while (auto sleeping_mask =
             sleeping_workers_.load(std::memory_order_relaxed)) {
    // We always give a preference to workers with a lower index (LSB in
    // `sleeping_mask`).
    //
    // If we're under light load, this way we hopefully can avoid wake workers
    // with higher index at all.
    auto last_sleeping = __builtin_ffsll(sleeping_mask) - 1;
    auto claiming_mask = 1ULL << last_sleeping;
    if (FLARE_LIKELY(sleeping_workers_.fetch_and(~claiming_mask,
                                                 std::memory_order_relaxed) &
                     claiming_mask)) {
      // We claimed the worker. Let's wake it up then.
      //
      // `WaitSlot` class it self guaranteed no wake-up loss. So don't worry
      // about that.
      FLARE_CHECK_LT(last_sleeping, group_size_);
      wait_slots_[last_sleeping].Wake();
      sleeping_worker_wakeups->Add(1);
      return true;
    }
    Pause();
  }
  return false;
}

}  // namespace flare::fiber::detail
