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

#ifndef FLARE_FIBER_DETAIL_FIBER_WORKER_H_
#define FLARE_FIBER_DETAIL_FIBER_WORKER_H_

#include <cstddef>

#include <queue>
#include <thread>

#include "flare/base/align.h"

namespace flare::fiber::detail {

struct FiberEntity;
class SchedulingGroup;

// A pthread worker for running fibers.
class alignas(hardware_destructive_interference_size) FiberWorker {
 public:
  FiberWorker(SchedulingGroup* sg, std::size_t worker_index);

  // Add foreign scheduling group for stealing.
  //
  // May only be called prior to `Start()`.
  void AddForeignSchedulingGroup(SchedulingGroup* sg,
                                 std::uint64_t steal_every_n);

  // Start the worker thread.
  //
  // If `no_cpu_migration` is set, this fiber worker is bind to
  // #`worker_index`-th processor in `affinity`.
  void Start(bool no_cpu_migration);

  // Wail until this worker quits.
  //
  // Note that there's no `Stop()` here. Instead, you should call
  // `SchedulingGroup::Stop()` to stop all the workers when exiting.
  void Join();

  // Non-copyable, non-movable.
  FiberWorker(const FiberWorker&) = delete;
  FiberWorker& operator=(const FiberWorker&) = delete;

 private:
  void WorkerProc();
  FiberEntity* StealFiber();

 private:
  struct Victim {
    SchedulingGroup* sg;
    std::uint64_t steal_every_n;
    std::uint64_t next_steal;

    // `std::priority_queue` orders elements descendingly.
    bool operator<(const Victim& v) const { return next_steal > v.next_steal; }
  };

  SchedulingGroup* sg_;
  std::size_t worker_index_;
  std::uint64_t steal_vec_clock_{};
  std::priority_queue<Victim> victims_;
  std::thread worker_;
};

}  // namespace flare::fiber::detail

#endif  // FLARE_FIBER_DETAIL_FIBER_WORKER_H_
