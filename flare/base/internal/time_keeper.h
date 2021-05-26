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

#ifndef FLARE_BASE_INTERNAL_TIME_KEEPER_H_
#define FLARE_BASE_INTERNAL_TIME_KEEPER_H_

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "flare/base/function.h"
#include "flare/base/never_destroyed.h"
#include "flare/base/ref_ptr.h"

namespace flare::internal {

// Several components in `flare/base` need to do their bookkeeping periodically.
// So as not to introduce a timer manager for each of them, we use this class to
// serve them all.
//
// Note that in general, components in `flare/base` should not use timer in
// `flare/fiber`. For other components, or user code, consider using fiber timer
// instead, this one is unlikely what you should use.
//
// This class performs poorly.
//
// NOT INTENDED FOR PUBLIC USE.
class TimeKeeper {
 public:
  static TimeKeeper* Instance();

  // DO NOT CALL THEM YOURSELF.
  //
  // `flare::Start(...)` will call them at appropriate time.
  void Start();
  void Stop();
  void Join();

  // Register a timer.
  //
  // If `is_slow_cb` is set, `cb` is called outside of timer thread. This helps
  // improving timeliness. (If this is the case, `cb` may be called
  // concurrently. We might address this issue in the future.)
  std::uint64_t AddTimer(std::chrono::steady_clock::time_point expires_at,
                         std::chrono::nanoseconds interval,
                         Function<void(std::uint64_t)> cb, bool is_slow_cb);

  void KillTimer(std::uint64_t timer_id);

 private:
  friend class NeverDestroyedSingleton<TimeKeeper>;
  struct Entry : RefCounted<Entry> {
    std::mutex lock;
    std::atomic<bool> cancelled{false};
    std::atomic<bool> is_slow_cb;
    std::atomic<bool> running{false};  // Applicable to slow callback only, to
                                       // avoid calls to `cb` concurrently.
    std::shared_ptr<Function<void(std::uint64_t)>> cb;
    std::chrono::steady_clock::time_point expires_at;
    std::chrono::nanoseconds interval;
  };
  using EntryPtr = RefPtr<Entry>;
  struct EntryPtrComp {
    bool operator()(const EntryPtr& x, const EntryPtr& y) const {
      return x->expires_at > y->expires_at;
    }
  };

  TimeKeeper();
  TimeKeeper(const TimeKeeper&) = delete;
  TimeKeeper& operator=(const TimeKeeper&) = delete;

  void WorkerProc();
  void FireFastTimer(EntryPtr ptr);
  void FireSlowTimer(EntryPtr ptr);

 private:
  // We use it to workaround global destruction order issue.
  //
  // If set, `KillTimer` will be effectively a no-op.
  static std::atomic<bool> exited_;

  std::mutex lock_;
  std::condition_variable cv_;
  std::priority_queue<EntryPtr, std::vector<EntryPtr>, EntryPtrComp> timers_;
  std::thread worker_;
};

}  // namespace flare::internal

#endif  // FLARE_BASE_INTERNAL_TIME_KEEPER_H_
