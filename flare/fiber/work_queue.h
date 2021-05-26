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

#ifndef FLARE_FIBER_WORK_QUEUE_H_
#define FLARE_FIBER_WORK_QUEUE_H_

#include <queue>

#include "flare/base/function.h"
#include "flare/fiber/condition_variable.h"
#include "flare/fiber/fiber.h"
#include "flare/fiber/mutex.h"

namespace flare::fiber {

// Each work queue consists of a dedicated fiber for running jobs.
//
// Work posted to this queue is run in a FIFO fashion.
class WorkQueue {
 public:
  WorkQueue();

  // Scheduling `cb` for execution.
  void Push(Function<void()>&& cb);

  // Stop the queue.
  void Stop();

  // Wait until all pending works has run.
  void Join();

 private:
  void WorkerProc();

 private:
  Fiber worker_;
  fiber::Mutex lock_;
  fiber::ConditionVariable cv_;
  std::queue<Function<void()>> jobs_;
  bool stopped_ = false;
};

}  // namespace flare::fiber

#endif  // FLARE_FIBER_WORK_QUEUE_H_
