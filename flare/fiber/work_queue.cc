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

#include "flare/fiber/work_queue.h"

#include <mutex>
#include <utility>

#include "flare/base/logging.h"

namespace flare::fiber {

WorkQueue::WorkQueue() {
  worker_ = Fiber([this] { WorkerProc(); });
}

void WorkQueue::Push(Function<void()>&& cb) {
  std::scoped_lock _(lock_);
  FLARE_CHECK(!stopped_, "The work queue is leaving.");
  jobs_.push(std::move(cb));
  cv_.notify_one();
}

void WorkQueue::Stop() {
  std::scoped_lock _(lock_);
  stopped_ = true;
  cv_.notify_one();
}

void WorkQueue::Join() { worker_.join(); }

void WorkQueue::WorkerProc() {
  while (true) {
    std::unique_lock lk(lock_);
    cv_.wait(lk, [&] { return stopped_ || !jobs_.empty(); });

    // So long as there still are pending jobs, we keep running.
    if (jobs_.empty()) {
      FLARE_CHECK(stopped_);
      break;
    }

    // We move all pending jobs out to reduce lock contention.
    auto temp = std::move(jobs_);
    lk.unlock();
    while (!temp.empty()) {
      auto&& job = temp.front();
      job();
      temp.pop();
    }
  }
}

}  // namespace flare::fiber
