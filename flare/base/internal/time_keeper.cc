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

#include "flare/base/internal/time_keeper.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <utility>

#include "flare/base/internal/background_task_host.h"
#include "flare/base/likely.h"

using namespace std::literals;

namespace flare::internal {

std::atomic<bool> TimeKeeper::exited_{false};

TimeKeeper* TimeKeeper::Instance() {
  static NeverDestroyedSingleton<TimeKeeper> tk;
  return tk.Get();
}

void TimeKeeper::Start() {
  worker_ = std::thread([&] { WorkerProc(); });
}

void TimeKeeper::Stop() {
  exited_.store(true, std::memory_order_relaxed);
  std::scoped_lock lk(lock_);
  cv_.notify_all();
}

void TimeKeeper::Join() { worker_.join(); }

std::uint64_t TimeKeeper::AddTimer(
    std::chrono::steady_clock::time_point expires_at,
    std::chrono::nanoseconds interval, Function<void(std::uint64_t)> cb,
    bool is_slow_cb) {
  if (FLARE_UNLIKELY(exited_.load(std::memory_order_relaxed))) {
    return -1;
  }
  auto ptr = MakeRefCounted<Entry>();
  auto timer_id = reinterpret_cast<std::uint64_t>(ptr.Get());
  ptr->cb = std::make_shared<Function<void(std::uint64_t)>>(std::move(cb));
  ptr->cancelled.store(false, std::memory_order_relaxed);
  ptr->is_slow_cb = is_slow_cb;
  ptr->expires_at = std::max(std::chrono::steady_clock::now(), expires_at);
  ptr->interval = interval;
  ptr->Ref();  // For return value.

  std::scoped_lock lk(lock_);
  timers_.push(std::move(ptr));
  cv_.notify_all();  // Always wake worker up, performance does not matter here.
  return timer_id;
}

void TimeKeeper::KillTimer(std::uint64_t timer_id) {
  RefPtr<Entry> ref(adopt_ptr, reinterpret_cast<Entry*>(timer_id));
  std::scoped_lock lk(ref->lock);
  ref->cb = nullptr;
  ref->cancelled.store(true, std::memory_order_relaxed);
}

TimeKeeper::TimeKeeper() = default;

void TimeKeeper::WorkerProc() {
  while (!exited_.load(std::memory_order_relaxed)) {
    std::unique_lock lk(lock_);
    auto pred = [&] {
      // `ReadSteadyClock()` is not available here. `flare/base:chrono` carries
      // a dependency on us.
      return !timers_.empty() &&
             timers_.top()->expires_at <= std::chrono::steady_clock::now();
    };
    if (exited_.load(std::memory_order_relaxed)) {
      break;
    }
    // Do NOT use `max()` here, otherwise overflow can occur (in std::).
    auto next = timers_.empty() ? std::chrono::steady_clock::now() + 100s
                                : timers_.top()->expires_at;
    cv_.wait_until(lk, next, pred);

    if (!pred()) {
      continue;
    }
    // Fire the first timer.
    auto p = timers_.top();
    timers_.pop();
    if (!p->is_slow_cb.load(std::memory_order_relaxed)) {
      FireFastTimer(std::move(p));
    } else {
      FireSlowTimer(std::move(p));
    }
  }
}

// Called with `lock_` held.
void TimeKeeper::FireFastTimer(EntryPtr ptr) {
  std::shared_ptr<Function<void(std::uint64_t)>> cb;
  {
    std::scoped_lock _(ptr->lock);
    cb = ptr->cb;
    if (ptr->cancelled.load(std::memory_order_relaxed)) {
      return;
    }
  }
  (*cb)(reinterpret_cast<std::uint64_t>(ptr.Get()));
  // Push it back with new `expires_at`.
  std::scoped_lock _(ptr->lock);
  if (ptr->cancelled.load(std::memory_order_relaxed)) {
    // No need to add it back then.
    return;
  }
  ptr->expires_at += ptr->interval;
  timers_.push(std::move(ptr));
}

void TimeKeeper::FireSlowTimer(EntryPtr ptr) {
  BackgroundTaskHost::Instance()->Queue([this, ptr] {
    std::shared_ptr<Function<void(std::uint64_t)>> cb;
    {
      std::scoped_lock _(ptr->lock);
      cb = ptr->cb;
      if (ptr->cancelled.load(std::memory_order_relaxed)) {
        return;
      }
    }
    (*cb)(reinterpret_cast<std::uint64_t>(ptr.Get()));

    // To prevent multiple calls to run concurrently, we don't add the timer
    // back until the callback has returned.
    std::scoped_lock _(lock_);
    std::scoped_lock __(ptr->lock);
    if (!ptr->cancelled.load(std::memory_order_relaxed)) {
      ptr->expires_at += ptr->interval;
      timers_.push(std::move(ptr));
      cv_.notify_all();
    }
  });
}

}  // namespace flare::internal
