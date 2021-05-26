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

#include "flare/base/thread/out_of_duty_callback.h"

#include <chrono>

#include "thirdparty/benchmark/benchmark.h"

using namespace std::literals;

// Not optimized yet. TODO(luobogao): Optimize it.

// Run on (76 X 2494.14 MHz CPU s)
// CPU Caches:
//   L1 Data 32K (x76)
//   L1 Instruction 32K (x76)
//   L2 Unified 4096K (x76)
// Load Average: 4.32, 7.69, 14.99
// ----------------------------------------------------------------
// Benchmark                      Time             CPU   Iterations
// ----------------------------------------------------------------
// Benchmark_Empty             6.19 ns         6.19 ns    112959275
// Benchmark_One               6.81 ns         6.81 ns    102790403
// Benchmark_OneThousand       8.67 ns         8.64 ns     82970175

namespace flare {

void Benchmark_Empty(benchmark::State& state) {
  while (state.KeepRunning()) {
    NotifyThreadOutOfDutyCallbacks();
  }
}

BENCHMARK(Benchmark_Empty);

void Benchmark_One(benchmark::State& state) {
  SetThreadOutOfDutyCallback([] {}, 1ms);
  while (state.KeepRunning()) {
    NotifyThreadOutOfDutyCallbacks();
  }
}

BENCHMARK(Benchmark_One);

void Benchmark_OneThousand(benchmark::State& state) {
  for (int i = 0; i != 1000; ++i) {
    SetThreadOutOfDutyCallback([] {}, 1ms);
  }
  while (state.KeepRunning()) {
    NotifyThreadOutOfDutyCallbacks();
  }
}

BENCHMARK(Benchmark_OneThousand);

}  // namespace flare
