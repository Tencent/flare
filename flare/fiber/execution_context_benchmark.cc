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

#include "flare/fiber/execution_context.h"

#include <string>

#include "benchmark/benchmark.h"

#include "flare/init.h"

// Run on (76 X 2494.14 MHz CPU s)
// CPU Caches:
//   L1 Data 32K (x76)
//   L1 Instruction 32K (x76)
//   L2 Unified 4096K (x76)
// Load Average: 40.73, 22.73, 18.24
// -----------------------------------------------------------
// Benchmark                 Time             CPU   Iterations
// -----------------------------------------------------------
// Benchmark_ELSGet       1.66 ns         1.66 ns    407762367

namespace flare::fiber {

ExecutionLocal<std::string> fls_str;

void Benchmark_ELSGet(benchmark::State& state) {
  ExecutionContext::Create()->Execute([&] {
    fls_str.UnsafeInit(new std::string(), [](auto p) { delete p; });

    while (state.KeepRunning()) {
      // <+227>: mov   %fs:0x0(%r13),%rdi
      // <+232>: cmp   $0x7,%rsi
      // <+236>: ja    0x4617e0
      // <+242>: lea   0xe0(%rdi,%rsi,8),%rax
      // <+250>: mov   (%rax),%rdi               # Pointer to current fiber.
      // <+253>: lea   0x4c1ef4(%rip),%rax       # `fls_str`.
      // <+260>: mov   (%rax),%rsi
      // <+263>: cmp   $0x7,%rsi
      // <+267>: ja    0x4617d0                  # ELS slow init & get.
      // <+269>: shl   $0x4,%rsi
      // <+273>: lea   0x8(%rdi,%rsi,1),%rax     # Indexing into `inline_els_`.
      // <+278>: mov   (%rax),%rax               # Pointer to `std::string`.
      // <+281>: test  %rax,%rax
      // <+284>: je    0x4617c0                  # Slow init `fls_str`.
      benchmark::DoNotOptimize(*fls_str);
    }
  });
}

BENCHMARK(Benchmark_ELSGet);

}  // namespace flare::fiber

int main(int argc, char** argv) {
  return flare::Start(argc, argv, [](auto argc, auto argv) {
    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
    ::benchmark::RunSpecifiedBenchmarks();
    return 0;
  });
}
