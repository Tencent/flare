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

#ifndef FLARE_IO_DETAIL_WATCHDOG_H_
#define FLARE_IO_DETAIL_WATCHDOG_H_

#include <atomic>
#include <thread>
#include <vector>

#include "flare/base/delayed_init.h"
#include "flare/base/thread/latch.h"

namespace flare {

class EventLoop;

}  // namespace flare

namespace flare::io::detail {

// `Watchdog` is responsible for monitoring healthiness of `EventLoop`s.
//
// If one (or more) `EventLoop` has not run for a sufficient long time,
// `Watchdog` has a right to crash the whole system.
class Watchdog {
 public:
  Watchdog();

  // Add a new `EventLoop` for watching.
  //
  // Thread-compatible. All `EventLoop`s must be added before calling `Start()`.
  void AddEventLoop(EventLoop* watched);

  void Start();
  void Stop();

  // Caveat: Even if `Join()` returns, it's well possible the task posted
  // to `EventLoop` by `*this` is being called (or even pending for being
  // called). And `*this` consequently could not be destroyed.
  //
  // `Stop()` & `Join()` the `EventLoop` before destroying `*this`.
  void Join();

 private:
  void WorkerProc();

 private:
  std::atomic<bool> exiting_{false};
  Latch exiting_latch_{1};  // For notify `WorkerProc()`.
  std::vector<EventLoop*> watched_;
  DelayedInit<std::thread> watcher_;
};

}  // namespace flare::io::detail

#endif  // FLARE_IO_DETAIL_WATCHDOG_H_
