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

#include "flare/base/thread/thread_local.h"

#include "benchmark/benchmark.h"

// Run on (76 X 2494.14 MHz CPU s)
// CPU Caches:
//   L1 Data 32K (x76)
//   L1 Instruction 32K (x76)
//   L2 Unified 4096K (x76)
// Load Average: 2.76, 3.56, 7.61
// -----------------------------------------------------------------------------
// Benchmark                                          Time       CPU  Iterations
// -----------------------------------------------------------------------------
// Benchmark_TlsAlwaysInitializedGet               2.44 ns   2.44 ns   285683009
// Benchmark_TlsAlwaysInitializedSet               2.34 ns   2.34 ns   299918457
// Benchmark_TlsAlwaysInitializedSet/threads:12   0.201 ns   2.35 ns   299573472
// Benchmark_TlsGet                                2.59 ns   2.59 ns   270215926

namespace flare {

volatile int v;

void Benchmark_TlsAlwaysInitializedGet(benchmark::State& state) {
  static internal::ThreadLocalAlwaysInitialized<int> tls;

  *tls.Get() = 0x12345678;
  while (state.KeepRunning()) {
    // <+84>:   mov  0x13499d(%rip),%rdi      # tls.offset_.
    // <+94>:   cmp  %rdi,%fs:(%r12)          # TLS object cache overrun?
    // <+99>:   jbe  0x40ff40                 # Slow path, reload cache.
    // <+101>:  mov  %fs:0x8(%r12),%rax       # TLS object base.
    // <+107>:  add  %rdi,%rax                # rax: Object pointer.
    // <+110>:  mov  (%rax),%edx              # edx: Value
    //
    // <+112>:  lea  0x134909(%rip),%rax      # `v`.
    // <+119>:  mov  %edx,(%rax)
    v = *tls.Get();
  }
}

BENCHMARK(Benchmark_TlsAlwaysInitializedGet);

static internal::ThreadLocalAlwaysInitialized<int> tls_mt;

void Benchmark_TlsAlwaysInitializedSet(benchmark::State& state) {
  while (state.KeepRunning()) {
    *tls_mt = 123;
  }
}

BENCHMARK(Benchmark_TlsAlwaysInitializedSet);
BENCHMARK(Benchmark_TlsAlwaysInitializedSet)->Threads(12);

void Benchmark_TlsGet(benchmark::State& state) {
  static ThreadLocal<int> tls;

  *tls.Get() = 0x12345678;
  while (state.KeepRunning()) {
    // <+108>:  mov  0x13470d(%rip),%rdi      # Same as above.
    // <+115>:  mov  %rax,(%rbx)
    // <+118>:  cmp  %rdi,%fs:(%r12)
    // <+123>:  jbe  0x410190
    // <+125>:  mov  %fs:0x8(%r12),%rax
    // <+131>:  add  %rdi,%rax
    // <+134>:  mov  (%rax),%rax              # rax: `unique_ptr`.
    // <+137>:  test %rax,%rax                # Initialized?
    // <+140>:  je   0x410180                 # Slow path, initialize TLS.
    // <+142>:  mov  (%rax),%edx              # edx: Value loaded.
    //
    // <+144>:  lea  0x1346c9(%rip),%rax      # `v`.
    // <+151>:  mov  %edx,(%rax)
    v = *tls.Get();
  }
}

BENCHMARK(Benchmark_TlsGet);

}  // namespace flare
