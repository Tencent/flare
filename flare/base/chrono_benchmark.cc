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

#include "flare/base/chrono.h"

#include <sys/time.h>

#include <chrono>

#include "benchmark/benchmark.h"

// Clock source: kvm-clock.
//
// Run on (76 X 2494.14 MHz CPU s)
// CPU Caches:
//   L1 Data 32K (x76)
//   L1 Instruction 32K (x76)
//   L2 Unified 4096K (x76)
// Load Average: 0.22, 0.62, 0.46
// --------------------------------------------------------------------------
// Benchmark                                Time             CPU   Iterations
// --------------------------------------------------------------------------
// Benchmark_GetTimeOfDay                66.1 ns         66.1 ns     10575869
// Benchmark_StdSteadyClock               114 ns          114 ns      6115946
// Benchmark_StdSystemClock               112 ns          112 ns      6269914
// Benchmark_ReadSteadyClock             67.9 ns         67.9 ns     10314219
// Benchmark_ReadSystemClock             67.4 ns         67.4 ns     10391789
// Benchmark_ReadCoarseSteadyClock       2.06 ns         2.06 ns    343728853
// Benchmark_ReadCoarseSystemClock       2.03 ns         2.03 ns    345874331

// Clock source: tsc
//
// Run on (76 X 2494.14 MHz CPU s)
// CPU Caches:
//   L1 Data 32K (x76)
//   L1 Instruction 32K (x76)
//   L2 Unified 4096K (x76)
// Load Average: 0.41, 0.62, 0.47
// --------------------------------------------------------------------------
// Benchmark                                Time             CPU   Iterations
// --------------------------------------------------------------------------
// Benchmark_GetTimeOfDay                26.4 ns         26.4 ns     26547543
// Benchmark_StdSteadyClock              90.2 ns         90.1 ns      7707191
// Benchmark_StdSystemClock              90.7 ns         90.7 ns      7717671
// Benchmark_ReadSteadyClock             27.0 ns         27.0 ns     25924588
// Benchmark_ReadSystemClock             27.4 ns         27.4 ns     25570753
// Benchmark_ReadCoarseSteadyClock       2.04 ns         2.04 ns    346416827
// Benchmark_ReadCoarseSystemClock       2.02 ns         2.02 ns    344582832

namespace flare {

void Benchmark_GetTimeOfDay(benchmark::State& state) {
  while (state.KeepRunning()) {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
  }
}

BENCHMARK(Benchmark_GetTimeOfDay);

void Benchmark_StdSteadyClock(benchmark::State& state) {
  while (state.KeepRunning()) {
    (void)std::chrono::steady_clock::now();
  }
}

BENCHMARK(Benchmark_StdSteadyClock);

void Benchmark_StdSystemClock(benchmark::State& state) {
  while (state.KeepRunning()) {
    (void)std::chrono::system_clock::now();
  }
}

BENCHMARK(Benchmark_StdSystemClock);

void Benchmark_ReadSteadyClock(benchmark::State& state) {
  while (state.KeepRunning()) {
    ReadSteadyClock();
  }
}

BENCHMARK(Benchmark_ReadSteadyClock);

void Benchmark_ReadSystemClock(benchmark::State& state) {
  while (state.KeepRunning()) {
    ReadSystemClock();
  }
}

BENCHMARK(Benchmark_ReadSystemClock);

void Benchmark_ReadCoarseSteadyClock(benchmark::State& state) {
  while (state.KeepRunning()) {
    // ... <+23>: lea 0x137c5a(%rip),%rax  # `system_clock_time_since_epoch`
    // ... <+30>: mov (%rax),%rax
    ReadCoarseSteadyClock();
  }
}

BENCHMARK(Benchmark_ReadCoarseSteadyClock);

void Benchmark_ReadCoarseSystemClock(benchmark::State& state) {
  while (state.KeepRunning()) {
    ReadCoarseSystemClock();
  }
}

BENCHMARK(Benchmark_ReadCoarseSystemClock);

}  // namespace flare
