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

#include "flare/base/function.h"

#include "benchmark/benchmark.h"

// Run on (76 X 2494.14 MHz CPU s)
// CPU Caches:
//   L1 Data 32K (x76)
//   L1 Instruction 32K (x76)
//   L2 Unified 4096K (x76)
// Load Average: 8.53, 29.14, 32.30
// ----------------------------------------------------------------
// Benchmark                      Time             CPU   Iterations
// ----------------------------------------------------------------
// Benchmark_New               2.16 ns         2.16 ns    323780004
// Benchmark_Assign            1.80 ns         1.80 ns    388247067
// Benchmark_AssignLarge       25.3 ns         25.3 ns     27699855
// Benchmark_AssignEmpty       2.16 ns         2.16 ns    324057211
// Benchmark_Invoke            1.80 ns         1.80 ns    388728737
// Benchmark_Move              7.32 ns         7.32 ns     96122547

// (With tcmalloc)
//
// Run on (76 X 2494.14 MHz CPU s)
// CPU Caches:
//   L1 Data 32K (x76)
//   L1 Instruction 32K (x76)
//   L2 Unified 4096K (x76)
// Load Average: 38.56, 47.08, 36.74
// ----------------------------------------------------------------
// Benchmark                      Time             CPU   Iterations
// ----------------------------------------------------------------
// Benchmark_New               1.80 ns         1.80 ns    388742350
// Benchmark_Assign            1.80 ns         1.80 ns    388590857
// Benchmark_AssignLarge       15.7 ns         15.7 ns     44710627
// Benchmark_AssignEmpty       2.16 ns         2.16 ns    323908140
// Benchmark_Invoke            1.80 ns         1.80 ns    388777521
// Benchmark_Move              7.14 ns         7.14 ns     97401425

namespace flare {

void Benchmark_New(benchmark::State& state) {
  while (state.KeepRunning()) {
    Function<void()> f = [] {};
    benchmark::DoNotOptimize(f);
  }
}

BENCHMARK(Benchmark_New);

void Benchmark_Assign(benchmark::State& state) {
  Function<void()> f;
  while (state.KeepRunning()) {
    f = [] {};
    benchmark::DoNotOptimize(f);
  }
}

BENCHMARK(Benchmark_Assign);

void Benchmark_AssignLarge(benchmark::State& state) {
  Function<void()> f;
  while (state.KeepRunning()) {
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-lambda-capture"
#endif
    f = [x = std::array<char, 64>()] {};
#ifdef __clang__
#pragma clang diagnostic pop
#endif
    benchmark::DoNotOptimize(f);
  }
}

BENCHMARK(Benchmark_AssignLarge);

void Benchmark_AssignEmpty(benchmark::State& state) {
  while (state.KeepRunning()) {
    Function<void()> f;
    f = [] {};
    benchmark::DoNotOptimize(f);
  }
}

BENCHMARK(Benchmark_AssignEmpty);

void Benchmark_Invoke(benchmark::State& state) {
  Function<int()> f = [] { return 1; };
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(f());
  }
}

BENCHMARK(Benchmark_Invoke);

void Benchmark_Move(benchmark::State& state) {
  Function<void()> f1 = [] {}, f2 = [] {};
  while (state.KeepRunning()) {
    // Three move.
    //
    // t = f1; f1 = f2; f2 = t;
    std::swap(f1, f2);
    benchmark::DoNotOptimize(f1);
    benchmark::DoNotOptimize(f2);
  }
}

BENCHMARK(Benchmark_Move);

}  // namespace flare
