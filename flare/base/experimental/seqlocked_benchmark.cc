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

#include "flare/base/experimental/seqlocked.h"

#include "benchmark/benchmark.h"

// x86-64 (Skylake):
//
// Run on (76 X 2494.14 MHz CPU s)
// CPU Caches:
//   L1 Data 32K (x76)
//   L1 Instruction 32K (x76)
//   L2 Unified 4096K (x76)
// Load Average: 8.07, 7.93, 5.02
// --------------------------------------------------------------
// Benchmark                    Time             CPU   Iterations
// --------------------------------------------------------------
// Benchmark_Seqlocked       1.67 ns         1.67 ns    420146026

// AArch64 (Neoverse N1?):
//
// Run on (32 X 200 MHz CPU s)
// Load Average: 3.19, 1.32, 6.13
// --------------------------------------------------------------
// Benchmark                    Time             CPU   Iterations
// --------------------------------------------------------------
// Benchmark_Seqlocked       7.71 ns         7.71 ns     90815080

namespace flare::experimental {

struct X {
  char buffer[32] = {};
};

Seqlocked<X> value;

void Benchmark_Seqlocked(benchmark::State& state) {
  while (state.KeepRunning()) {
    value.Load();
  }
}

BENCHMARK(Benchmark_Seqlocked);

}  // namespace flare::experimental
