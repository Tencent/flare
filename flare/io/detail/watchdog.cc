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

#include "flare/io/detail/watchdog.h"

#include <chrono>
#include <memory>
#include <vector>

#include "thirdparty/gflags/gflags.h"

#include "flare/base/chrono.h"
#include "flare/base/thread/latch.h"
#include "flare/io/event_loop.h"

using namespace std::literals;

DEFINE_int32(
    flare_watchdog_check_interval, 10000,
    "Interval between two run of the watch dog, in milliseconds. This value "
    "should be at least as large as `flare_watchdog_maximum_tolerable_delay`.");

DEFINE_int32(flare_watchdog_maximum_tolerable_delay, 5000,
             "Maximum delay between watch dog posted its callback and its "
             "callback being called, in milliseconds.");

DEFINE_bool(flare_watchdog_crash_on_unresponsive, false,
            "If set, watchdog will crash the whole program if it thinks it's "
            "not responsive. Otherwise an error is logged instead.");

namespace flare::io::detail {

Watchdog::Watchdog() = default;

void Watchdog::AddEventLoop(EventLoop* watched) { watched_.push_back(watched); }

void Watchdog::Start() {
  watcher_.Init([&] { return WorkerProc(); });
}

void Watchdog::Stop() {
  exiting_.store(true, std::memory_order_relaxed);
  exiting_latch_.count_down();
}

void Watchdog::Join() { watcher_->join(); }

void Watchdog::WorkerProc() {
  FLARE_CHECK_GE(FLAGS_flare_watchdog_check_interval,
                 FLAGS_flare_watchdog_maximum_tolerable_delay);
  auto next_try = ReadSteadyClock();

  // For every `FLAGS_flare_watchdog_check_interval` second, we post a task to
  // `EventLoop`, and check if it get run within
  // `FLAGS_flare_watchdog_maximum_tolerable_delay` seconds.
  while (!exiting_.load(std::memory_order_relaxed)) {
    auto wait_until =
        ReadSteadyClock() + FLAGS_flare_watchdog_maximum_tolerable_delay * 1ms;
    // Be careful here, when `flare_watchdog_crash_on_unresponsive` is disabled,
    // `acked` can be `count_down`-d **after** we leave the scope.
    //
    // We use `std::shared_ptr` here to address this.
    std::vector<std::shared_ptr<Latch>> acked(watched_.size());

    for (std::size_t index = 0; index != watched_.size(); ++index) {
      acked[index] = std::make_shared<Latch>(1);

      // Add a task to the `EventLoop` and check (see below) if it get run in
      // time.
      watched_[index]->AddTask([acked = acked[index]] { acked->count_down(); });
    }

    // This loop may not be merged with the above one, as it may block (and
    // therefore delays subsequent calls to `EventLoop::AddTask`.).
    for (std::size_t index = 0; index != watched_.size(); ++index) {
      bool responsive = acked[index]->wait_until(wait_until) ||
                        exiting_.load(std::memory_order_relaxed);
      if (!responsive) {
        if (FLAGS_flare_watchdog_crash_on_unresponsive) {
          FLARE_LOG_FATAL(
              "Event loop {} is likely unresponsive. Crashing the program.",
              fmt::ptr(watched_[index]));
        } else {
          FLARE_LOG_ERROR("Event loop {} is likely unresponsive. Overloaded?",
                          fmt::ptr(watched_[index]));
        }
      }
    }
    VLOG(10) << "Watchdog: Life is good.";

    // Sleep until next round starts.
    next_try += FLAGS_flare_watchdog_check_interval * 1ms;
    exiting_latch_.wait_until(next_try);
  }
}

}  // namespace flare::io::detail
