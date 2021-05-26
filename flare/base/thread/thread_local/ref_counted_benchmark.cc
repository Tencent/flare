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

#include "flare/base/thread/thread_local/ref_counted.h"

#include "thirdparty/benchmark/benchmark.h"

// Run on (76 X 2494.14 MHz CPU s)
// CPU Caches:
//   L1 Data 32K (x76)
//   L1 Instruction 32K (x76)
//   L2 Unified 4096K (x76)
// Load Average: 7.52, 10.18, 8.81
// ------------------------------------------------------------------
// Benchmark                        Time             CPU   Iterations
// ------------------------------------------------------------------
// Benchmark_RefCountedGet       1.94 ns         1.94 ns    360696033

namespace flare {

struct C : public RefCounted<C> {
  int v = 123;
};

void Benchmark_RefCountedGet(benchmark::State& state) {
  static internal::ThreadLocalRefCounted<C> tls;

  tls->v = 12345;
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(tls->v);
  }
}

BENCHMARK(Benchmark_RefCountedGet);

}  // namespace flare
