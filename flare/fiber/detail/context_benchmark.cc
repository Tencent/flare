// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "thirdparty/benchmark/benchmark.h"

// Note: We do two context switches each round in `Benchmark_JumpContext`, the
// actual cost of `jump_context` is half of the timings shown below.

// Intel SKL:
//
// Run on (76 X 2494.14 MHz CPU s)
// CPU Caches:
//   L1 Data 32K (x76)
//   L1 Instruction 32K (x76)
//   L2 Unified 4096K (x76)
// Load Average: 1.05, 1.70, 1.94
// ----------------------------------------------------------------
// Benchmark                      Time             CPU   Iterations
// ----------------------------------------------------------------
// Benchmark_MakeContext       2.18 ns         2.17 ns    322769840
// Benchmark_JumpContext       10.6 ns         10.6 ns     65779317

// ARM Neoverse N1 (2.6 GHz)
//
// Run on (16 X 200 MHz CPU s)
// Load Average: 0.17, 0.06, 0.04
// ----------------------------------------------------------------
// Benchmark                      Time             CPU   Iterations
// ----------------------------------------------------------------
// Benchmark_MakeContext       2.67 ns         2.67 ns    258713996
// Benchmark_JumpContext       30.1 ns         30.1 ns     23277665

// IBM POWER8 (Frequency unknown, DVFS enabled.).
//
// Run on (44 X 3524 MHz CPU s)
// CPU Caches:
//   L1 Data 64 KiB (x22)
//   L1 Instruction 32 KiB (x22)
//   L2 Unified 512 KiB (x22)
//   L3 Unified 8192 KiB (x22)
// Load Average: 4.91, 6.08, 6.70
// ----------------------------------------------------------------
// Benchmark                      Time             CPU   Iterations
// ----------------------------------------------------------------
// Benchmark_MakeContext       8.71 ns         8.71 ns     80484985
// Benchmark_JumpContext       74.5 ns         74.5 ns      9446046

// Defined in `flare/fiber/detail/{arch}/*.S`
extern "C" {

void jump_context(void** self, void* to, void* context);
void* make_context(void* sp, std::size_t size, void (*start_proc)(void*));
}

namespace flare::fiber::detail {

void Benchmark_MakeContext(benchmark::State& state) {
  char stack_buffer[4096];
  while (state.KeepRunning()) {
    make_context(stack_buffer + 4096, 4096, nullptr);
  }
}

BENCHMARK(Benchmark_MakeContext);

void* master;
void* child;

void Benchmark_JumpContext(benchmark::State& state) {
  char ctx[4096];
  child = make_context(ctx + 4096, 4096, [](void*) {
    while (true) {
      jump_context(&child, master, nullptr);
    }
  });
  while (state.KeepRunning()) {
    // Note that we do two context switch each round (one to `child`, and one
    // back to `master`.).
    jump_context(&master, child, nullptr);
  }
}

BENCHMARK(Benchmark_JumpContext);

}  // namespace flare::fiber::detail
