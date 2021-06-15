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

#include "flare/base/hazptr.h"

#include <atomic>

#include "benchmark/benchmark.h"

// Run on (76 X 2494.14 MHz CPU s)
// CPU Caches:
//   L1 Data 32K (x76)
//   L1 Instruction 32K (x76)
//   L2 Unified 4096K (x76)
// Load Average: 0.43, 0.31, 0.34
// -----------------------------------------------------------------
// Benchmark                       Time             CPU   Iterations
// -----------------------------------------------------------------
// Benchmark_HazptrReader       3.72 ns         3.72 ns    188240896

namespace flare {

struct Buffer : public HazptrObject<Buffer> {};

std::atomic<Buffer*> buffer_ptr = std::make_unique<Buffer>().release();
volatile const void* ppp;

void Benchmark_HazptrReader(benchmark::State& state) {
  while (state.KeepRunning()) {
    // Construction of `Hazptr`.
    //
    // <+34>:  mov    %fs:(%rbx),%rax
    // <+38>:  cmp    %fs:0x8(%rbx),%rax
    // <+43>:  jbe    0x411b10                 # EntryCache::GetSlow
    // <+45>:  lea    -0x8(%rax),%rdx
    // <+49>:  mov    %rdx,%fs:(%rbx)
    // <+53>:  mov    -0x8(%rax),%rax
    // <+57>:  lea    0x16d3c0(%rip),%rcx      # `buffer_ptr`
    // <+64>:  mov    (%rcx),%rdx
    // <+67>:  mov    %rdx,(%rax)              # Store into `hazptr.entry_`
    // <+70>:  mov    (%rcx),%rsi              # Reload
    //                                         # (Light barrier)
    // <+73>:  cmp    %rdx,%rsi
    // <+76>:  jne    0x411b00                 # Retry.

    // Destruction of `Hazptr`.
    //
    // <+78>:  test   %rax,%rax                # Is it empty? (Why bother
    //                                         # checking this?)
    // <+81>:  je     0x411a7a
    // <+83>:  movq   $0x0,(%rax)              # Clear `hazptr.entry_`
    // <+90>:  mov    %fs:(%rbx),%rdx
    // <+94>:  cmp    %fs:0x10(%rbx),%rdx
    // <+99>:  jae    0x411b28                 # EntryCache::PutSlow
    // <+101>: lea    0x8(%rdx),%rcx
    // <+105>: mov    %rcx,%fs:(%rbx)
    // <+109>: mov    %rax,(%rdx)

    Hazptr hazptr;
    auto p = hazptr.Keep(&buffer_ptr);
    benchmark::DoNotOptimize(p);
  }
}

BENCHMARK(Benchmark_HazptrReader);

}  // namespace flare
