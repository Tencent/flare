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

#ifndef FLARE_BASE_INTERNAL_THREAD_POOL_H_
#define FLARE_BASE_INTERNAL_THREAD_POOL_H_

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "flare/base/function.h"

namespace flare::internal {

// FOR INTERNAL USE ONLY.
//
// If you want to run something concurrently, start a fiber by `Async` instead.
// This task pool is NOT OPTIMIZED AT ALL, and almost always fails to perform
// comparatively to fiber runtime.
class ThreadPool {
 public:
  explicit ThreadPool(std::size_t workers, const std::vector<int>& affinity,
                      int nice_value);
  void QueueJob(Function<void()>&& job);
  void Stop();
  void Join();

 private:
  void WorkerProc();

 private:
  std::atomic<bool> exiting_{false};
  std::vector<std::thread> workers_;

  std::mutex lock_;
  std::condition_variable cv_;
  std::queue<Function<void()>> jobs_;
};

}  // namespace flare::internal

#endif  // FLARE_BASE_INTERNAL_THREAD_POOL_H_
