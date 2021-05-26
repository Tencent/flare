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

#ifndef FLARE_FIBER_DETAIL_SCHEDULING_GROUP_H_
#define FLARE_FIBER_DETAIL_SCHEDULING_GROUP_H_

#include <atomic>
#include <chrono>
#include <cstddef>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "flare/base/align.h"
#include "flare/base/delayed_init.h"
#include "flare/base/exposed_var.h"
#include "flare/base/function.h"
#include "flare/base/internal/annotation.h"
#include "flare/base/logging.h"
#include "flare/base/thread/spinlock.h"
#include "flare/fiber/detail/run_queue.h"
#include "flare/fiber/detail/timer_worker.h"  // Uhhhh..

namespace flare::fiber::detail {

struct FiberEntity;
class TimerWorker;

// Each scheduling group consists of a group of pthread worker and exactly one
// timer worker (who is responsible for, timers).
//
// `SchedulingGroup` itself is merely a data structure, it's `FiberWorker` &
// `TimerWorker`'s responsibility for running fibers / timers.
class alignas(hardware_destructive_interference_size) SchedulingGroup {
 public:
  // Guard value for marking the scheduling group is shutting down.
  inline static FiberEntity* const kSchedulingGroupShuttingDown =
      reinterpret_cast<FiberEntity*>(0x1);

  // Worker index for timer worker.
  inline static constexpr std::size_t kTimerWorkerIndex = -1;

  // Construct a scheduling group consisting of `size` pthread workers (not
  // including the timer worker.).
  explicit SchedulingGroup(const std::vector<int>& affinity, std::size_t size);

  // We define destructor in `.cc` so as to use incomplete type in member
  // fields.
  ~SchedulingGroup();

  // Get current scheduling group.
  static SchedulingGroup* Current() noexcept { return current_; }

  // Get scheduling group the given timer belongs to.
  static SchedulingGroup* GetTimerOwner(std::uint64_t timer_id) noexcept {
    return TimerWorker::GetTimerOwner(timer_id)->GetSchedulingGroup();
  }

  // Acquire a (ready) fiber to run. Any memory modification done by the fiber
  // returned when it's pushed into scheduling queue (by `ReadyFiber`) are
  // visible to the caller.
  //
  // Returns `nullptr` if there's none.
  //
  // Returns `kSchedulingGroupShuttingDown` if the entire scheduling group is
  // shutting down *and* there's no ready fiber to run.
  FiberEntity* AcquireFiber() noexcept;

  // Spin and try to acquire a fiber.
  FiberEntity* SpinningAcquireFiber() noexcept;

  // Sleep until at least one fiber is ready for run or the entire scheduling
  // group is shutting down.
  //
  // The method can return spuriously, i.e., return `nullptr` even if it should
  // keep sleeping instead.
  FiberEntity* WaitForFiber() noexcept;

  // Acquire a fiber. The calling thread does not belong to this scheduling
  // group (i.e., it's stealing a fiber.).
  //
  // Returns `nullptr` if there's none. This method never returns
  // `kSchedulingGroupShuttingDown`.
  FiberEntity* RemoteAcquireFiber() noexcept;

  // Schedule fibers in [start, end) to run in batch.
  //
  // No scheduling lock should be held by the caller, and all fibers to be
  // scheduled mustn't be run before (i.e., this is the fiber time they're
  // pushed into run queue.).
  //
  // Provided for perf. reasons. The same behavior can be achieved by calling
  // `ReadyFiber` multiple times.
  //
  // CAUTION: `scheduling_group_local` is NOT respected by this method. (FIXME.)
  //
  // TODO(luobogao): `span<FiberEntity*>` seems to be superior.
  void StartFibers(FiberEntity** start, FiberEntity** end) noexcept;

  // Schedule a fiber to run.
  //
  // `FiberEntity::scheduler_lock` of `fiber` must be held by the caller. This
  // helps you prevent race between call to this method and call to `Halt()`.
  //
  // Special case: `scheduler_lock` can be empty if `fiber` has never been run,
  // in this case there is no race possible that should be prevented by
  // `scheduler_lock`.
  void ReadyFiber(FiberEntity* fiber,
                  std::unique_lock<Spinlock>&& scheduler_lock) noexcept;

  // Halt caller fiber.
  //
  // The caller need to be woken by someone else explicitly via `ReadyFiber`.
  //
  // `FiberEntity::scheduler_lock` of `fiber` must be held by the caller. This
  // helps you prevent race between call to this method and call to
  // `ReadyFiber()`.
  //
  // `self` must be caller's `FiberEntity*`.
  void Halt(FiberEntity* self,
            std::unique_lock<Spinlock>&& scheduler_lock) noexcept;

  // Yield pthread worker to someone else.
  //
  // The caller must not be added to run queue by anyone else (either
  // concurrently or prior to this call). It will be added to run queue by this
  // method.
  //
  // `self->scheduler_lock` must NOT be held.
  //
  // The caller is automatically added to run queue by this method. (@sa:
  // `Halt`).
  void Yield(FiberEntity* self) noexcept;

  // Yield pthread worker to the specified fiber.
  //
  // Both `self` and `to` must not be added to run queue by anyone else (either
  // concurrently or prior to this call). They'll be add to run queue by this
  // method.
  //
  // Scheduler lock of `self` and `to` must NOT be held.
  //
  // The caller is automatically added to run queue for run by other workers.
  void SwitchTo(FiberEntity* self, FiberEntity* to) noexcept;

  // Create a (not-scheduled-yet) timer. You must enable it later via
  // `EnableTimer`.
  //
  // Timer ID returned is also given to timer callback on called.
  //
  // Timer ID returned must be either detached via `DetachTimer` or freed (the
  // timer itself is cancelled in if freed) via `RemoveTimer`. Otherwise a leak
  // will occur.
  //
  // The reason why we choose a two-step way to set up a timer is that, in
  // certain cases, the timer's callback may want to access timer's ID stored
  // somewhere (e.g., some global data structure). If creating timer and
  // enabling it is done in a single step, the user will have a hard time to
  // synchronizes between timer-creator and timer-callback.
  //
  // This method can only be called inside **this** scheduling group's fiber
  // worker context.
  //
  // Note that `cb` is called in timer worker's context, you normally would want
  // to fire a fiber for run your own logic.
  [[nodiscard]] std::uint64_t CreateTimer(
      std::chrono::steady_clock::time_point expires_at,
      Function<void(std::uint64_t)>&& cb) {
    FLARE_CHECK(timer_worker_);
    FLARE_CHECK_EQ(Current(), this);
    return timer_worker_->CreateTimer(expires_at, std::move(cb));
  }

  // Periodic timer.
  [[nodiscard]] std::uint64_t CreateTimer(
      std::chrono::steady_clock::time_point initial_expires_at,
      std::chrono::nanoseconds interval, Function<void(std::uint64_t)>&& cb) {
    FLARE_CHECK(timer_worker_);
    FLARE_CHECK_EQ(Current(), this);
    return timer_worker_->CreateTimer(initial_expires_at, interval,
                                      std::move(cb));
  }

  // Enable a timer. Timer's callback can be called even before this method
  // returns.
  void EnableTimer(std::uint64_t timer_id) {
    FLARE_CHECK(timer_worker_);
    FLARE_CHECK_EQ(Current(), this);
    return timer_worker_->EnableTimer(timer_id);
  }

  // Detach a timer.
  void DetachTimer(std::uint64_t timer_id) noexcept {
    FLARE_CHECK(timer_worker_);
    return timer_worker_->DetachTimer(timer_id);
  }

  // Cancel a timer set by `SetTimer`.
  //
  // Callable in any thread belonging to the same scheduling group.
  //
  // If the timer is being fired (or has fired), this method does nothing.
  void RemoveTimer(std::uint64_t timer_id) noexcept {
    FLARE_CHECK(timer_worker_);
    return timer_worker_->RemoveTimer(timer_id);
  }

  // Workers (including timer worker) call this to join this scheduling group.
  //
  // Several thread-local variables used by `SchedulingGroup` is initialize
  // here.
  //
  // After calling this method, `Current` is usable.
  void EnterGroup(std::size_t index);

  // You're calling this on thread exit.
  void LeaveGroup();

  // Get number of *pthread* workers (not including the timer worker) in this
  // scheduling group.
  std::size_t GroupSize() const noexcept;

  // CPU affinity of this scheduling group, or an empty vector if not specified.
  const std::vector<int>& Affinity() const noexcept;

  // Set timer worker. This method must be called before registering pthread
  // workers.
  //
  // This worker is later used by `SetTimer` for setting timers.
  //
  // `SchedulingGroup` itself guarantees if it calls methods on `worker`, the
  // caller pthread is one of the pthread worker in this scheduling group.
  // (i.e., the number of different caller thread equals to `GroupSize()`.
  //
  // On shutting down, the `TimerWorker` must be joined before joining pthread
  // workers, otherwise use-after-free can occur.
  void SetTimerWorker(TimerWorker* worker) noexcept;

  // Shutdown the scheduling group.
  //
  // All further calls to `SetTimer` / `DispatchFiber` leads to abort.
  //
  // Wake up all workers blocking on `WaitForFiber`, once all ready fiber has
  // terminated, all further calls to `AcquireFiber` returns
  // `kSchedulingGroupShuttingDown`.
  //
  // It's still your responsibility to shut down pthread workers / timer
  // workers. This method only mark this control structure as being shutting
  // down.
  void Stop();

 private:
  bool WakeUpOneWorker() noexcept;
  bool WakeUpWorkers(std::size_t n) noexcept;
  bool WakeUpOneSpinningWorker() noexcept;
  bool WakeUpOneDeepSleepingWorker() noexcept;

 private:
  static constexpr auto kUninitializedWorkerIndex =
      std::numeric_limits<std::size_t>::max();
  FLARE_INTERNAL_TLS_MODEL static inline thread_local SchedulingGroup* current_;
  FLARE_INTERNAL_TLS_MODEL static thread_local std::size_t worker_index_;

  class WaitSlot;

  std::atomic<bool> stopped_{false};
  std::size_t group_size_;
  TimerWorker* timer_worker_ = nullptr;
  std::vector<int> affinity_;

  // Exposes internal state.
  DelayedInit<ExposedVarDynamic<std::string>> spinning_workers_var_,
      sleeping_workers_var_;

  // Ready fibers are put here.
  RunQueue run_queue_;

  // Fiber workers sleep on this.
  std::unique_ptr<WaitSlot[]> wait_slots_;

  // Bit mask.
  //
  // We carefully chose to use 1 to represent "spinning" and "sleeping", instead
  // of "running" and "awake", here. This way if the number of workers is
  // smaller than 64, those not-used bit is treated as if they represent running
  // workers, and don't need special treat.
  alignas(hardware_destructive_interference_size)
      std::atomic<std::uint64_t> spinning_workers_{0};
  alignas(hardware_destructive_interference_size)
      std::atomic<std::uint64_t> sleeping_workers_{0};

  // Set if the last spinner successfully grabbed a fiber to run. In this case
  // we're likely under load, so it set this flag for other spinners (if there
  // is one) to wake more workers up (and hopefully to get a fiber or spin).
  alignas(hardware_destructive_interference_size)
      std::atomic<bool> pending_spinner_wakeup_{false};
};

}  // namespace flare::fiber::detail

#endif  // FLARE_FIBER_DETAIL_SCHEDULING_GROUP_H_
