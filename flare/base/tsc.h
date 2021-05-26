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

#ifndef FLARE_BASE_TSC_H_
#define FLARE_BASE_TSC_H_

// Let me be clear: Generally, you SHOULDN'T use TSC as timestamp in the first
// place:
//
// - If you need precise timestamp, consider using `ReadXxxClock()` (@sa:
//   `chrono.h`),
// - If you need to read timestamp fast enough (but can tolerate a lower
//   resolution), use `ReadCoarseXxxClock()`).
//
// There are simply too many subtleties in using TSC as timestamp. Don't do
// this unless you're perfectly clear what you're doing.
//
// @sa: flare/doc/timestamp.md
// @sa: http://oliveryang.net/2015/09/pitfalls-of-TSC-usage/
//
// YOU HAVE BEEN WARNED.

#if defined(__x86_64__) || defined(__amd64__)
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif
#endif

#include <chrono>
#include <cinttypes>
#include <utility>

#include "flare/base/chrono.h"
#include "flare/base/internal/annotation.h"
#include "flare/base/internal/logging.h"
#include "flare/base/likely.h"

namespace flare {

namespace tsc::detail {

extern const std::chrono::nanoseconds kNanosecondsPerUnit;
#if defined(__aarch64__)
// On AArch64 we determine system timer's frequency via `cntfrq_el0`, this
// "unit" is merely used when mapping timer counter to wall clock.
constexpr std::uint32_t kUnit = 1 * 1024 * 1024;
#else
constexpr std::uint32_t kUnit = 4 * 1024 * 1024;  // ~2ms on 2GHz.
#endif

// Returns: Steady-clock, TSC.
std::pair<std::chrono::steady_clock::time_point, std::uint64_t>
ReadConsistentTimestamps();

}  // namespace tsc::detail

// Note that TSC is almost guaranteed not to synchronized among all cores (since
// you're likely running on a multi-chip system.). If you need such a timestamp,
// use `std::steady_clock` instead. TSC is suitable for situations where
// accuracy can be trade for speed (however, in this case you should check
// `ReadCoarseSteadyClock()` out.).
#if defined(__x86_64__) || \
    defined(__MACHINEX86_X64) /* TODO(luobogao): Introduce FLARE_AMD64.*/
inline std::uint64_t ReadTsc() { return __rdtsc(); }
#elif defined(__aarch64__)
inline std::uint64_t ReadTsc() {
  // Sub-100MHz resolution (exactly 100MHz in our environment), not as accurate
  // as x86-64 though.
  std::uint64_t result;
  asm volatile("mrs %0, cntvct_el0" : "=r"(result));
  return result;
}
#elif defined(__powerpc__)
inline std::uint64_t ReadTsc() { return __builtin_ppc_get_timebase(); }
#else
#error Unsupported architecture.
#endif

// Subtract two TSCs.
//
// This method take special care for possible TSC going backwards, and return 0
// if that is the case.
//
// No, `constant_tsc` helps little here, especially if you try to pass TSC from
// one core to another.
inline std::uint64_t TscElapsed(std::uint64_t start, std::uint64_t to) {
  // If you want to change the condition below, check comments in
  // `TimestampFromTsc` first.
  if (FLARE_UNLIKELY(start >= to)) {
    // TSC can go backwards if (not a comprehensive list):
    //
    // - `start` and `to` was captured in different NUMA node.
    // - OoO engine tricked us. (e.g., `start` is loaded from another core and
    //   `to` is read locally. It's possible that reading `to` finished before
    //   our cache read request even reaches the core holding `start`. In this
    //   case, reading `start` can happen after reading `to`.)
    //
    // We need to compasenate this case.

#if defined(__x86_64__)
    FLARE_LOG_WARNING_IF_EVERY_N(
        start - to > 1'000'000, 100,
        "Unexpected: TSC goes backward for more than 1M cycles (0.5ms on "
        "2GHz). You will likely see some unreasonable timestamps. Called with "
        "start = {}, to = {}.",
        start, to);
#elif defined(__aarch64__)
    FLARE_LOG_WARNING_IF_EVERY_N(
        start - to > 100'000, 100,
        "Unexpected: TSC goes backward for more than 2ms. You will likely see "
        "some unreasonable timestamps. Called with start = {}, to = {}.",
        start, to);
#else
    FLARE_LOG_WARNING_IF_EVERY_N(
        start - to > 1'000'000, 100,
        "Unexpected: TSC goes backward for quite a while. You will likely see "
        "some unreasonable timestamps. Called with start = {}, to = {}.",
        start, to);
#endif
    return 0;
  }
  return to - start;
}

// `TscElapsedSince(...)`?

// Converts difference between two TSCs into `std::duration<...>`.
//
// Note that this method is PRONE TO (INTERNAL) ARITHMETIC OVERFLOW. Depending
// on the platform you're using, you'll likely see incorrect result if more than
// a few thousands of seconds have passed since `start` to `to`.
//
// As a rule of thumb, you should only consider using this method (after you've
// taken all the quirks about using TSC, @sa: doc/timestamp.md, into
// consideration) for small time intervals.
inline std::chrono::nanoseconds DurationFromTsc(std::uint64_t start,
                                                std::uint64_t to) {
  return TscElapsed(start, to) * tsc::detail::kNanosecondsPerUnit /
         tsc::detail::kUnit;
}

// Converts TSC to timestamp of `steady_clock`.
inline std::chrono::steady_clock::time_point TimestampFromTsc(
    std::uint64_t tsc) {
  // TSC is not guaranteed to be consistent between NUMA domains.
  //
  // Here we use different base timestamp for different threads, so long as
  // threads are not migrated between NUMA domains, we're fine. (We'll be in a
  // little trouble if cross-NUMA migration do occur, but it's addressed below).
  FLARE_INTERNAL_TLS_MODEL thread_local std::pair<
      std::chrono::steady_clock::time_point, std::uint64_t>
      kFutureTimestamp;

  // **EXACTLY** the same condition as the one tested in `TscElapsed`. This
  // allows the compiler to optimize the two `if`s into the same one in fast
  // path.
  //
  // What's more, this also resolves the follow issue:
  //
  // - This removes the conditional initialization of `Start`, as it can be
  //   statically initialized at thread creation now.
  // - We move `Start.second` forward a bit each round, so `Start` is updated
  //   periodically. This compensates the inaccuracy in `DurationFromTsc` (even
  //   if it provides high resolution, it doesn't provide very good accuracy.).
  //   This also addresses TSC inconsistency between NUMA nodes.
  //
  // And all of these come with no price: the `if` in `DurationFromTsc` is
  // always needed, we just "replaced" that `if` with our own. So in fast path,
  // it's exactly the same code get executed.
  if (FLARE_UNLIKELY(tsc >= kFutureTimestamp.second)) {
    kFutureTimestamp = tsc::detail::ReadConsistentTimestamps();
  }
  return kFutureTimestamp.first - DurationFromTsc(tsc, kFutureTimestamp.second);
}

}  // namespace flare

#endif  // FLARE_BASE_TSC_H_
