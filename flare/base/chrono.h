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

#ifndef FLARE_BASE_CHRONO_H_
#define FLARE_BASE_CHRONO_H_

#include <atomic>
#include <chrono>
#include <thread>

#include "flare/base/align.h"

namespace flare {

namespace detail::chrono {

// The two timestamps are always updated at almost the same time, there's no
// point in putting them into separate cachelines, so we group them together
// here.
struct alignas(hardware_destructive_interference_size)
    AsynchronouslyUpdatedTimestamps {
  std::atomic<std::chrono::nanoseconds> steady_clock_time_since_epoch;
  std::atomic<std::chrono::nanoseconds> system_clock_time_since_epoch;
};

// Not so accurate.
inline AsynchronouslyUpdatedTimestamps async_updated_timestamps;

inline struct CoarseClockInitializer {
 public:
  CoarseClockInitializer();
  ~CoarseClockInitializer();

 private:
  std::thread worker_;
  std::atomic<bool> exiting_{false};
} coarse_clock_initializer;

}  // namespace detail::chrono

// Originally bypassed `std::chrono::steady_clock::now()` for performance on
// CentOS6 where libstdc++ could fall back to a real `syscall` via
// `_GLIBCXX_USE_CLOCK_GETTIME_SYSCALL`. That optimisation is unnecessary on
// any modern toolchain (vDSO is universal now), and using `clock_gettime`
// directly risks clock epoch mismatch:
//
// On macOS (Darwin 24 / macOS 15), `CLOCK_MONOTONIC` and
// `std::chrono::steady_clock::now()` disagree on epoch — `CLOCK_MONOTONIC`
// can be ahead by accumulated sleep/suspend time. Mixing time_points from
// the two clocks (extracting `time_since_epoch()` and reconstructing a
// `steady_clock::time_point`) silently shifts absolute time, which broke
// `TimerWorker::cv_.wait_until()`.
inline std::chrono::steady_clock::time_point ReadSteadyClock() {
  return std::chrono::steady_clock::now();
}

inline std::chrono::system_clock::time_point ReadSystemClock() {
  return std::chrono::system_clock::now();
}

// This method is faster than `ReadSteadyClock`, but it only provide a precision
// in (several) milliseconds (deviates less than 10ms.).
//
// This clock is not read by `clock_gettime(CLOCK_XXX_COARSE)`, but instead,
// directly from `steady_clock_time_since_epoch`.
//
// This is inspired by section 3.5 of tRPC's perf. optimization document.
inline std::chrono::steady_clock::time_point ReadCoarseSteadyClock() {
  return std::chrono::steady_clock::time_point(
      detail::chrono::async_updated_timestamps.steady_clock_time_since_epoch
          .load(std::memory_order_relaxed));
}

// Same as `ReadCoarseSteadyClock` except it's for `std::system_clock`.
inline std::chrono::system_clock::time_point ReadCoarseSystemClock() {
  return std::chrono::system_clock::time_point(
      std::chrono::duration_cast<std::chrono::system_clock::duration>(
          detail::chrono::async_updated_timestamps.system_clock_time_since_epoch
              .load(std::memory_order_relaxed)));
}

// Read UNIX timestamp, i.e., seconds since 01/01/1970.
inline /* signed integer */ std::int64_t ReadUnixTimestamp() {
  return ReadCoarseSystemClock().time_since_epoch() / std::chrono::seconds(1);
}

}  // namespace flare

#endif  // FLARE_BASE_CHRONO_H_
