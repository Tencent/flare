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

#include "flare/base/tsc.h"

#include "benchmark/benchmark.h"

// Run on (76 X 2494.14 MHz CPU s)
// 2020-03-03 20:53:04
// Benchmark                           Time           CPU Iterations
// -----------------------------------------------------------------
// Benchmark_ReadTsc                  10 ns         10 ns   69476465
// Benchmark_DurationFromTsc           2 ns          2 ns  290462314
// Benchmark_TimestampFromTsc          2 ns          2 ns  324148675

namespace flare {

volatile int start = 10, end = 20;
volatile int x;
volatile std::uint64_t tsc0 = ReadTsc();

void Benchmark_ReadTsc(benchmark::State& state) {
  while (state.KeepRunning()) {
    x = ReadTsc();  // Incurs memory access overhead (cache hit, though).
  }
}

BENCHMARK(Benchmark_ReadTsc);

void Benchmark_DurationFromTsc(benchmark::State& state) {
  while (state.KeepRunning()) {
    // ... <+34>: lea    0x1066d7(%rip),%rax  # `end`
    // ... <+41>: lea    0x1066d4(%rip),%rdx  # `start`
    // ... <+48>: movslq (%rax),%rax
    // ... <+51>: movslq (%rdx),%rdx
    // ... <+54>: cmp    %rdx,%rax
    // ... <+57>: jbe    0x409db0             # Slow path in `DurationFromTsc`
    // ... <+59>: sub    %rdx,%rax
    // ... <+62>: lea    0x106c7b(%rip),%rdx  # `kNanosecondsPerUnit`
    // ... <+69>: imul   (%rdx),%rax
    // ... <+73>: shr    $0x1c,%rax
    // ... <+77>: lea    0x106c64(%rip),%rdx  # `x`
    // ... <+84>: mov    %eax,(%rdx)
    x = DurationFromTsc(start, end).count();
  }
}

BENCHMARK(Benchmark_DurationFromTsc);

void Benchmark_TimestampFromTsc(benchmark::State& state) {
  while (state.KeepRunning()) {
    // ... <+69>:  lea    0x1512cc(%rip),%rax  # `tsc0`
    // ... <+76>:  mov    $0xffffffffffffffd0,%r14
    // ... <+83>:  mov    (%rax),%r13
    // ... <+86>:  mov    %fs:0x8(%r14),%rbx
    // ... <+91>:  cmp    %rbx,%r13
    // ... <+94>:  jae    0x40ef00             # Slow path for `DurationFromTsc`
    //                                         # and `TimestampFromTsc`
    // ... <+96>:  lea    0x1512d1(%rip),%rdx  # `kNanosecondsPerUnit`
    // ... <+103>: sub    %r13,%rbx
    // ... <+106>: imul   (%rdx),%rbx
    // ... <+110>: mov    %fs:(%r14),%rax
    // ... <+114>: shr    $0x1c,%rbx
    // ... <+118>: sub    %rbx,%rax
    // ... <+121>: lea    0x1512a0(%rip),%rdx  # `x`
    // ... <+128>: mov    %eax,(%rdx)
    x = TimestampFromTsc(tsc0).time_since_epoch().count();
  }
}

BENCHMARK(Benchmark_TimestampFromTsc);

}  // namespace flare
