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

#include "flare/fiber/fiber_local.h"

#include "benchmark/benchmark.h"

#include "flare/init.h"

// Run on (76 X 2494.14 MHz CPU s)
// CPU Caches:
//   L1 Data 32K (x76)
//   L1 Instruction 32K (x76)
//   L2 Unified 4096K (x76)
// Load Average: 0.44, 0.47, 1.53
// -----------------------------------------------------------
// Benchmark                 Time             CPU   Iterations
// -----------------------------------------------------------
// Benchmark_FLSGet       1.94 ns         1.94 ns    359310943

namespace flare {

// FLS tested below is "inlined" case.
//
// It's unfortunate that we only benchmarked `FiberLocal::Get()` but not
// allocating new FLS. I haven't come up with a way to benchmarking allocating
// (for non-trivial types) new FLS without starting new fiber (which is too
// heavy to make the benchmarking meaningful for FLS itself).

FiberLocal<int*> fls_ptr;

void Benchmark_FLSGet(benchmark::State& state) {
  while (state.KeepRunning()) {
    // <+23>: lea  0x4c145a(%rip),%rax        # `fls_ptr`
    // <+30>: mov  (%rax),%rsi
    // <+33>: mov  $0xffffffffffffb9e0,%rax   # Current fiber.
    // <+40>: mov  %fs:(%rax),%rdi
    // <+44>: cmp  $0x7,%rsi                  # index < inline FLS slots size?
    // <+48>: ja   0x461230                   # Slow path.
    // <+50>: lea  0xe0(%rdi,%rsi,8),%rax     # Got it.
    benchmark::DoNotOptimize(*fls_ptr);
  }
}

BENCHMARK(Benchmark_FLSGet);

}  // namespace flare

int main(int argc, char** argv) {
  return flare::Start(argc, argv, [](auto argc, auto argv) {
    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
    ::benchmark::RunSpecifiedBenchmarks();
    return 0;
  });
}
