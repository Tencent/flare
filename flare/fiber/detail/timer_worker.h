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

#ifndef FLARE_FIBER_DETAIL_TIMER_WORKER_H_
#define FLARE_FIBER_DETAIL_TIMER_WORKER_H_

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "flare/base/align.h"
#include "flare/base/function.h"
#include "flare/base/ref_ptr.h"
#include "flare/base/thread/latch.h"

namespace flare {

template <class>
struct PoolTraits;

}  // namespace flare

namespace flare::fiber::detail {

class SchedulingGroup;

// This class contains a dedicated pthread for running timers.
class alignas(hardware_destructive_interference_size) TimerWorker {
 public:
  explicit TimerWorker(SchedulingGroup* sg);
  ~TimerWorker();

  static TimerWorker* GetTimerOwner(std::uint64_t timer_id);

  // Create a timer. It's enabled separately via `EnableTimer`.
  //
  // @sa: `SchedulingGroup::CreateTimer`
  std::uint64_t CreateTimer(std::chrono::steady_clock::time_point expires_at,
                            Function<void(std::uint64_t)>&& cb);
  std::uint64_t CreateTimer(
      std::chrono::steady_clock::time_point initial_expires_at,
      std::chrono::nanoseconds interval, Function<void(std::uint64_t)>&& cb);

  // Enable a timer created before.
  void EnableTimer(std::uint64_t timer_id);

  // Cancel a timer.
  void RemoveTimer(std::uint64_t timer_id);

  // Detach a timer. This method can be helpful in fire-and-forget use cases.
  void DetachTimer(std::uint64_t timer_id);

  SchedulingGroup* GetSchedulingGroup();

  // Caller MUST be one of the pthread workers belong to the same scheduling
  // group.
  void InitializeLocalQueue(std::size_t worker_index);

  // Start the worker thread and join the given scheduling group.
  void Start();

  // Stop & Join.
  void Stop();
  void Join();

  // Non-copyable, non-movable.
  TimerWorker(const TimerWorker&) = delete;
  TimerWorker& operator=(const TimerWorker&) = delete;

 private:
  struct Entry;
  using EntryPtr = RefPtr<Entry>;
  struct ThreadLocalQueue;

  void WorkerProc();

  void AddTimer(EntryPtr timer);

  // Wait until all worker has called `InitializeLocalQueue()`.
  void WaitForWorkers();

  void ReapThreadLocalQueues();
  void FireTimers();
  void WakeWorkerIfNeeded(
      std::chrono::steady_clock::time_point local_expires_at);

  static ThreadLocalQueue* GetThreadLocalQueue();

 private:
  struct EntryPtrComp {
    bool operator()(const EntryPtr& p1, const EntryPtr& p2) const;
  };
  friend struct PoolTraits<Entry>;

  std::atomic<bool> stopped_{false};
  SchedulingGroup* sg_;
  flare::Latch latch;  // We use it to wait for workers' registration.

  // Pointer to thread-local timer vectors.
  std::vector<ThreadLocalQueue*> producers_;

  // Unfortunately GCC 8.2 does not support
  // `std::atomic<std::chrono::steady_clock::time_point>` (this is conforming,
  // as that specialization is not defined by the Standard), so we need to store
  // `time_point::time_since_epoch()` here.
  std::atomic<std::chrono::steady_clock::duration> next_expires_at_{
      std::chrono::steady_clock::duration::max()};
  std::priority_queue<EntryPtr, std::vector<EntryPtr>, EntryPtrComp> timers_;

  std::thread worker_;

  // `WorkerProc` sleeps on this.
  std::mutex lock_;
  std::condition_variable cv_;
};

}  // namespace flare::fiber::detail

#endif  // FLARE_FIBER_DETAIL_TIMER_WORKER_H_
