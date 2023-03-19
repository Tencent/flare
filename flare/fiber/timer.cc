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
#include "flare/fiber/fiber.h"
#include "flare/fiber/runtime.h"

namespace flare::fiber {

std::uint64_t SetTimer(std::chrono::steady_clock::time_point at,
                       Function<void()>&& cb) {
  return SetTimer(at, [cb = std::move(cb)](auto) { cb(); });
}

std::uint64_t SetTimer(std::chrono::steady_clock::time_point at,
                       Function<void(std::uint64_t)>&& cb) {
  auto sg = detail::NearestSchedulingGroup();
  auto timer_id =
      sg->CreateTimer(at, [cb = std::move(cb)](auto timer_id) mutable {
        // Note that we're called in timer's worker thread, not in fiber
        // context. So fire a fiber to run user's code.
        internal::StartFiberDetached(
            [cb = std::move(cb), timer_id] { cb(timer_id); });
      });
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
      // If `remaining` was non-zero, there must be a fiber running concurrently
      // to us, and is executing user's callback. In this case, that fiber will
      // see our increment and call user's callback for us.
      if (remaining.fetch_add(1, std::memory_order_acquire) == 0) {
        do {
          cb(tid);
          // If `remaining` was not 1, there must have been another fiber else
          // who tried to call user's callback (@see the increment above) and
          // gave up due to the presence of us. In this case, we're responsible
          // for calling user's callback on behalf of that fiber.
        } while (remaining.fetch_sub(1, std::memory_order_release) != 1);
      }
    }
    Function<void(std::uint64_t)> cb;
    std::atomic<std::int64_t> remaining{};
  };

  auto ucb = std::make_shared<UserCallback>();
  ucb->cb = std::move(cb);

  auto sg = detail::NearestSchedulingGroup();
  auto timer_id = sg->CreateTimer(at, interval, [ucb](auto tid) mutable {
    internal::StartFiberDetached([ucb, tid] { ucb->Run(tid); });
  });
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
