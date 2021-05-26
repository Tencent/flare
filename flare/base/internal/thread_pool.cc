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

#include "flare/base/internal/thread_pool.h"

#include <unistd.h>
#include <mutex>
#include <utility>
#include <vector>

#include "flare/base/internal/cpu.h"
#include "flare/base/internal/logging.h"
#include "flare/base/thread/attribute.h"

namespace flare::internal {

ThreadPool::ThreadPool(std::size_t workers, const std::vector<int>& affinity,
                       int nice_value) {
  workers_.resize(workers);
  for (auto&& t : workers_) {
    t = std::thread([=] {
      if (nice_value) {
        FLARE_PCHECK(nice(nice_value));
      }
      if (!affinity.empty()) {
        SetCurrentThreadAffinity(affinity);
      }
      WorkerProc();
    });
  }
}

void ThreadPool::QueueJob(Function<void()>&& job) {
  std::scoped_lock _(lock_);
  jobs_.push(std::move(job));
  cv_.notify_one();
}

void ThreadPool::Stop() {
  std::scoped_lock _(lock_);
  exiting_.store(true, std::memory_order_relaxed);
  cv_.notify_all();
}

void ThreadPool::Join() {
  for (auto&& t : workers_) {
    t.join();
  }
}

void ThreadPool::WorkerProc() {
  while (true) {
    std::unique_lock lk(lock_);
    cv_.wait(lk, [&] {
      return exiting_.load(std::memory_order_relaxed) || !jobs_.empty();
    });
    if (exiting_.load(std::memory_order_relaxed)) {
      break;
    }
    FLARE_CHECK(!jobs_.empty());
    auto job = std::move(jobs_.front());
    jobs_.pop();
    lk.unlock();
    job();
  }
}

}  // namespace flare::internal
