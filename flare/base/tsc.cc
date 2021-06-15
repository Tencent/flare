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

#include "flare/base/tsc.h"

#include <algorithm>
#include <chrono>
#include <iterator>
#include <utility>

#include "glog/raw_logging.h"

#include "flare/base/chrono.h"

using namespace std::literals;

namespace flare::tsc::detail {

#ifdef __aarch64__
const std::chrono::nanoseconds kNanosecondsPerUnit = [] {
  std::uint64_t freq;
  asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));
  return (kUnit * (1s / 1ns) / freq) * 1ns;
}();
#else
const std::chrono::nanoseconds kNanosecondsPerUnit = [] {
  int retries = 0;

  while (true) {
    // We try to determine the result multiple times, and filter out the
    // outliers from the result, so as to be as accurate as possible.
    constexpr auto kTries = 64;
    std::chrono::nanoseconds elapsed[kTries];

    for (int i = 0; i != kTries; ++i) {
      auto tsc0 = ReadTsc();
      auto start = ReadSteadyClock();
      while (ReadTsc() - tsc0 < kUnit) {
        // NOTHING.
      }
      elapsed[i] = ReadSteadyClock() - start;
    }
    std::sort(std::begin(elapsed), std::end(elapsed));

    // Only results in the middle are used.
    auto since = kTries / 3, upto = kTries / 3 * 2;
    if (elapsed[upto] - elapsed[since] > 1us) {  // I think it's big enough.
      if (++retries > 5) {
        RAW_LOG(WARNING,
                "We keep failing in determining TSC rate. Busy system?");
      }
      continue;
    }

    // The average of the results in the middle is the final result.
    std::chrono::nanoseconds sum{};
    for (int i = since; i != upto; ++i) {
      sum += elapsed[i];
    }
    return sum / (upto - since);
  }
}();
#endif

std::pair<std::chrono::steady_clock::time_point, std::uint64_t>
ReadConsistentTimestamps() {
  // Maximum difference between two calls to `ReadSteadyClock()`.
#ifndef __powerpc64__
  // I'm not aware of any system on which `clock_gettime` would consistently
  // consume more than 200ns.
  constexpr auto kTimestampInconsistencyTolerance = 1us;
#else
  // Well ppc64le processors are weird, at least the one I tested Flare on.
  //
  // Indeed on idle two consecutive calls to `clock_gettime` can still finishes
  // in less than 1us. Yet in case the system is busy, it can cost more.
  constexpr auto kTimestampInconsistencyTolerance = 10us;
#endif

  int retries = 0;

  while (true) {
    // Wall clock is read twice to detect preemption by other threads. We need
    // wall clock and TSC to be close enough to be useful.
    auto s1 = ReadSteadyClock();
    auto tsc = ReadTsc();
    auto s2 = ReadSteadyClock();

    if (s2 - s1 > kTimestampInconsistencyTolerance) {
      FLARE_LOG_WARNING_IF(
          ++retries > 5,
          "We're continually being preempted. Something might be wrong.");
      continue;  // We were preempted during execution, `tsc` and `s1` are
                 // likely to be incoherent. Try again then.
    }

    return std::pair(s1 + (s2 - s1) / 2 + tsc::detail::kNanosecondsPerUnit,
                     tsc + tsc::detail::kUnit);
  }
}

}  // namespace flare::tsc::detail
