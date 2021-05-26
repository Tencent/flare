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

#include "flare/fiber/timer.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <utility>

#include "flare/fiber/detail/scheduling_group.h"
#include "flare/fiber/execution_context.h"
#include "flare/fiber/fiber.h"
#include "flare/fiber/runtime.h"

namespace flare::fiber {

std::uint64_t SetTimer(std::chrono::steady_clock::time_point at,
                       Function<void()>&& cb) {
  return SetTimer(at, [cb = std::move(cb)](auto) { cb(); });
}

std::uint64_t SetTimer(std::chrono::steady_clock::time_point at,
                       Function<void(std::uint64_t)>&& cb) {
  auto ec = RefPtr(ref_ptr, ExecutionContext::Current());
  auto mcb = [cb = std::move(cb), ec = std::move(ec)](auto timer_id) mutable {
    // Note that we're called in timer's worker thread, not in fiber
    // context. So fire a fiber to run user's code.
    internal::StartFiberDetached(
        Fiber::Attributes{.execution_context = ec.Get()},
        [cb = std::move(cb), timer_id] { cb(timer_id); });
  };

  auto sg = detail::NearestSchedulingGroup();
  auto timer_id = sg->CreateTimer(at, std::move(mcb));
  sg->EnableTimer(timer_id);
  return timer_id;
}

std::uint64_t SetTimer(std::chrono::steady_clock::time_point at,
                       std::chrono::nanoseconds interval,
                       Function<void()>&& cb) {
  return SetTimer(at, interval, [cb = std::move(cb)](auto) { cb(); });
}

std::uint64_t SetTimer(std::chrono::steady_clock::time_point at,
                       std::chrono::nanoseconds interval,
                       Function<void(std::uint64_t)>&& cb) {
  // This is ugly. But since we have to start a fiber each time user's `cb` is
  // called, we must share it.
  //
  // We also take measures not to call user's callback before the previous call
  // has returned. Otherwise we'll likely crash user's (presumably poor) code.
  struct UserCallback {
    void Run(std::uint64_t tid) {
      if (!running.exchange(true, std::memory_order_acq_rel)) {
        cb(tid);
      }
      running.store(false, std::memory_order_relaxed);
      // Otherwise this call is lost. This can happen if user's code runs too
      // slowly. For the moment we left the behavior as unspecified.
    }
    Function<void(std::uint64_t)> cb;
    std::atomic<bool> running{};
  };

  auto ucb = std::make_shared<UserCallback>();
  auto ec = RefPtr(ref_ptr, ExecutionContext::Current());
  ucb->cb = std::move(cb);

  auto mcb = [ucb, ec = std::move(ec)](auto tid) mutable {
    internal::StartFiberDetached(
        Fiber::Attributes{.execution_context = ec.Get()},
        [ucb, tid] { ucb->cb(tid); });
  };

  auto sg = detail::NearestSchedulingGroup();
  auto timer_id = sg->CreateTimer(at, interval, std::move(mcb));
  sg->EnableTimer(timer_id);
  return timer_id;
}

std::uint64_t SetTimer(std::chrono::nanoseconds interval,
                       Function<void()>&& cb) {
  return SetTimer(ReadSteadyClock() + interval, interval, std::move(cb));
}

std::uint64_t SetTimer(std::chrono::nanoseconds interval,
                       Function<void(std::uint64_t)>&& cb) {
  return SetTimer(ReadSteadyClock() + interval, interval, std::move(cb));
}

void DetachTimer(std::uint64_t timer_id) {
  return detail::SchedulingGroup::GetTimerOwner(timer_id)->DetachTimer(
      timer_id);
}

void SetDetachedTimer(std::chrono::steady_clock::time_point at,
                      Function<void()>&& cb) {
  DetachTimer(SetTimer(at, std::move(cb)));
}

void SetDetachedTimer(std::chrono::steady_clock::time_point at,
                      std::chrono::nanoseconds interval,
                      Function<void()>&& cb) {
  DetachTimer(SetTimer(at, interval, std::move(cb)));
}

void KillTimer(std::uint64_t timer_id) {
  return detail::SchedulingGroup::GetTimerOwner(timer_id)->RemoveTimer(
      timer_id);
}

TimerKiller::TimerKiller() = default;

TimerKiller::TimerKiller(std::uint64_t timer_id) : timer_id_(timer_id) {}

TimerKiller::TimerKiller(TimerKiller&& tk) noexcept : timer_id_(tk.timer_id_) {
  tk.timer_id_ = 0;
}

TimerKiller& TimerKiller::operator=(TimerKiller&& tk) noexcept {
  Reset();
  timer_id_ = std::exchange(tk.timer_id_, 0);
  return *this;
}

TimerKiller::~TimerKiller() { Reset(); }

void TimerKiller::Reset(std::uint64_t timer_id) {
  if (auto tid = std::exchange(timer_id_, 0)) {
    KillTimer(tid);
  }
  timer_id_ = timer_id;
}

namespace internal {

[[nodiscard]] std::uint64_t CreateTimer(
    std::chrono::steady_clock::time_point at,
    Function<void(std::uint64_t)>&& cb) {
  return detail::NearestSchedulingGroup()->CreateTimer(at, std::move(cb));
}

[[nodiscard]] std::uint64_t CreateTimer(
    std::chrono::steady_clock::time_point at, std::chrono::nanoseconds interval,
    Function<void(std::uint64_t)>&& cb) {
  return detail::NearestSchedulingGroup()->CreateTimer(at, interval,
                                                       std::move(cb));
}

void EnableTimer(std::uint64_t timer_id) {
  detail::SchedulingGroup::GetTimerOwner(timer_id)->EnableTimer(timer_id);
}

void KillTimer(std::uint64_t timer_id) {
  return detail::SchedulingGroup::GetTimerOwner(timer_id)->RemoveTimer(
      timer_id);
}

}  // namespace internal

}  // namespace flare::fiber
