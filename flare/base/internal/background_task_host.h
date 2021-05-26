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

#ifndef FLARE_BASE_INTERNAL_BACKGROUND_TASK_HOST_H_
#define FLARE_BASE_INTERNAL_BACKGROUND_TASK_HOST_H_

#include <memory>
#include <vector>

#include "flare/base/function.h"
#include "flare/base/internal/thread_pool.h"
#include "flare/base/never_destroyed.h"

namespace flare::internal {

// To run some background task.
//
// For internal use only. INCORRECT USE OF THIS CLASS CAN ACTUALLY *DECREASE*
// OVERALL PERFORMANCE.
//
// TODO(luobogao): Make the task host "nice" by increasing its niceness (in
// Linux's terminology.).
class BackgroundTaskHost {
 public:
  static BackgroundTaskHost* Instance();

  void Start();
  void Stop();
  void Join();

  // Queue a job for execute asynchronously.
  //
  // CAUTION: There's absolutely NO GUARANTEE on timeliness of calling `op`.
  // Don't rely on that.
  //
  // The first overload choose NUMA domain **RANDOMLY**.
  void Queue(Function<void()>&& op);
  void Queue(std::uint64_t numa_id, Function<void()>&& op);

 private:
  friend class NeverDestroyedSingleton<BackgroundTaskHost>;
  BackgroundTaskHost();

 private:
  std::vector<std::unique_ptr<ThreadPool>> pools_;  // One per NUMA node.
};

}  // namespace flare::internal

#endif  // FLARE_BASE_INTERNAL_BACKGROUND_TASK_HOST_H_
