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

#include "flare/base/object_pool.h"

#include <chrono>

#include "benchmark/benchmark.h"

// Run on (76 X 2494.14 MHz CPU s)
// CPU Caches:
//   L1 Data 32K (x76)
//   L1 Instruction 32K (x76)
//   L2 Unified 4096K (x76)
// Load Average: 0.46, 0.35, 1.22
// ---------------------------------------------------------------------
// Benchmark                           Time             CPU   Iterations
// ---------------------------------------------------------------------
// Benchmark_ObjectPoolGetPut       3.88 ns         3.88 ns    179792926

using namespace std::literals;

namespace flare {

struct C {};

template <>
struct PoolTraits<C> {
  static constexpr auto kType = PoolType::MemoryNodeShared;
  static constexpr auto kLowWaterMark = 0;
  static constexpr auto kHighWaterMark = 10;
  static constexpr auto kMaxIdle = 10s;
  static constexpr auto kMinimumThreadCacheSize = 0;
  static constexpr auto kTransferBatchSize = 10;
};

void Benchmark_ObjectPoolGetPut(benchmark::State& state) {
  while (state.KeepRunning()) {
    // Code generated for `object_pool::Get()`.
    //
    // <+40>:   mov   %fs:0x10(%rbx),%rax       # Load `local`.
    // <+45>:   test  %rax,%rax                 # Initialized?
    // <+48>:   je    0x41b518                  # `InitializeOptAndGetSlow`
    // <+50>:   mov   0x8(%rax),%rdx
    // <+54>:   cmp   (%rax),%rdx               # `current_` == `objects_`?
    // <+57>:   je    0x41b518                  # `InitializeOptAndGetSlow`
    // <+59>:   lea   -0x10(%rdx),%rcx          # --current_
    // <+63>:   mov   %rcx,0x8(%rax)
    // <+67>:   mov   -0x10(%rdx),%rax          # Object from pool.

    // Code generated for `PooledPtr<T>::~PooledPtr`
    //
    // <+71>:   test  %rax,%rax
    // <+74>:   je    0x41b490

    // Following are code generated for `object_pool::Put()`.
    //
    // <+76>:   mov   %fs:0x10(%rbx),%rdx       # Load `local`.
    // <+81>:   test  %rdx,%rdx
    // <+84>:   je    0x41b520                  # `InitializeOptAndPutSlow`.
    // <+86>:   mov   0x8(%rdx),%rcx
    // <+90>:   cmp   0x10(%rdx),%rcx           # `full()`?
    // <+94>:   je    0x41b520                  # `InitializeOptAndPutSlow`.
    // <+96>:   mov   %fs:(%rbx),%rsi           # `desc`.
    // <+100>:  lea   0x10(%rcx),%rdi           # RDI = current_ + 1
    // <+104>:  mov   0x10(%rsi),%rsi           # RSI = destroy
    // <+108>:  mov   %rdi,0x8(%rdx)            # current_ = RDI
    // <+112>:  mov   %rax,(%rcx)               # `ptr`.
    // <+119>:  mov   %rsi,0x8(%rcx)            # `TypeDesc.destroy`.

    // Returns a `RecyclePtr<T>`, which is immediately destroyed, and implicitly
    // calls `object_pool::Put<C>(ptr)`.
    object_pool::Get<C>();
  }
}

BENCHMARK(Benchmark_ObjectPoolGetPut);

}  // namespace flare
