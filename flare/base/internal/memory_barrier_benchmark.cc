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

#include "flare/base/internal/memory_barrier.h"

#include "thirdparty/benchmark/benchmark.h"

// Run on (76 X 2494.14 MHz CPU s)
// CPU Caches:
//   L1 Data 32K (x76)
//   L1 Instruction 32K (x76)
//   L2 Unified 4096K (x76)
// Load Average: 0.79, 0.69, 0.52
// ---------------------------------------------------------------------------
// Benchmark                                 Time             CPU   Iterations
// ---------------------------------------------------------------------------
// Benchmark_MemoryBarrier                6.50 ns         6.50 ns    107734780
// Benchmark_Mfence                       15.7 ns         15.7 ns     44463362
// Benchmark_AsymmetricBarrierLight       1.82 ns         1.82 ns    384423477

namespace flare::internal {

void Benchmark_MemoryBarrier(benchmark::State& state) {
  while (state.KeepRunning()) {
    MemoryBarrier();
  }
}

BENCHMARK(Benchmark_MemoryBarrier);

void Benchmark_Mfence(benchmark::State& state) {
  while (state.KeepRunning()) {
    __sync_synchronize();
  }
}

BENCHMARK(Benchmark_Mfence);

void Benchmark_AsymmetricBarrierLight(benchmark::State& state) {
  while (state.KeepRunning()) {
    AsymmetricBarrierLight();
  }
}

BENCHMARK(Benchmark_AsymmetricBarrierLight);

}  // namespace flare::internal
