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

#include "flare/base/write_mostly/basic_ops.h"

#include "benchmark/benchmark.h"

// Run on (76 X 2494.14 MHz CPU s)
// CPU Caches:
//   L1 Data 32K (x76)
//   L1 Instruction 32K (x76)
//   L2 Unified 4096K (x76)
// Load Average: 2.33, 3.38, 7.40
// --------------------------------------------------------------------------
// Benchmark                                Time             CPU   Iterations
// --------------------------------------------------------------------------
// Benchmark_CounterAdd                  2.94 ns         2.94 ns    238091910
// Benchmark_CounterAdd/threads:12      0.248 ns         2.95 ns    237667920
// Benchmark_MaxerUpdate                 2.98 ns         2.94 ns    254692234

namespace flare {

void Benchmark_CounterAdd(benchmark::State& state) {
  static WriteMostlyCounter<int> adder;
  while (state.KeepRunning()) {
    adder.Add(1);
  }
}

BENCHMARK(Benchmark_CounterAdd);
BENCHMARK(Benchmark_CounterAdd)->Threads(12);

void Benchmark_MaxerUpdate(benchmark::State& state) {
  WriteMostlyMaxer<int> maxer;
  while (state.KeepRunning()) {
    maxer.Update(1);
  }
}

BENCHMARK(Benchmark_MaxerUpdate);

}  // namespace flare
