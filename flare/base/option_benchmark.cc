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

#include "flare/base/option.h"

#include "thirdparty/benchmark/benchmark.h"
#include "thirdparty/gflags/gflags.h"

// Run on (76 X 2494.14 MHz CPU s)
// CPU Caches:
//   L1 Data 32K (x76)
//   L1 Instruction 32K (x76)
//   L2 Unified 4096K (x76)
// Load Average: 0.50, 0.68, 0.76
// -----------------------------------------------------------
// Benchmark                 Time             CPU   Iterations
// -----------------------------------------------------------
// Benchmark_Int          2.03 ns         2.03 ns    346333209
// Benchmark_String       1.90 ns         1.90 ns    367972610

DEFINE_int32(int_test, 0, "");
DEFINE_string(string_test, "", "");

namespace flare {

GflagsOptions<int> opt_int("int_test");
GflagsOptions<std::string> opt_str("string_test");

void Benchmark_Int(benchmark::State& state) {
  option::OptionService::Instance()->ResolveAll();
  while (state.KeepRunning()) {
    // <+39>: lea    0x154932(%rip),%rax
    // <+46>: mov    0x18(%rax),%eax
    benchmark::DoNotOptimize(opt_int.Get());
  }
}

BENCHMARK(Benchmark_Int);

void Benchmark_String(benchmark::State& state) {
  while (state.KeepRunning()) {
    // <+47>: mov    0x30(%r12),%eax            # @sa: `ThreadCached`
    // <+52>: mov    $0xffffffffffffffb8,%rdx
    // <+59>: cmp    %fs:(%rdx),%rax
    // <+63>: jae    0x423a98
    // <+65>: shl    $0x6,%rax
    // <+69>: add    %fs:0x8(%rdx),%rax
    // <+74>: mov    (%rax),%rbx
    // <+77>: test   %rbx,%rbx
    // <+80>: je     0x423ab0
    // <+82>: mov    0x20(%r12),%rax
    // <+87>: cmp    %rax,(%rbx)
    // <+90>: jne    0x423ae0
    // <+92>: mov    0x8(%rbx),%rax
    benchmark::DoNotOptimize(opt_str.Get());
  }
}

BENCHMARK(Benchmark_String);

}  // namespace flare
