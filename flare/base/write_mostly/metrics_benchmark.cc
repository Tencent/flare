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

#include "flare/base/write_mostly/metrics.h"

#include "thirdparty/benchmark/benchmark.h"

// Run on (76 X 2494.14 MHz CPU s)
// CPU Caches:
//   L1 Data 32K (x76)
//   L1 Instruction 32K (x76)
//   L2 Unified 4096K (x76)
// Load Average: 1.11, 33.57, 64.22
// ------------------------------------------------------------------
// Benchmark                        Time             CPU   Iterations
// ------------------------------------------------------------------
// Benchmark_MetricsUpdate       10.1 ns         10.1 ns     69393819

namespace flare::write_mostly {

void Benchmark_MetricsReport(benchmark::State& state) {
  WriteMostlyMetrics<int> metrics;
  while (state.KeepRunning()) {
    metrics.Report(1);
  }
}

BENCHMARK(Benchmark_MetricsReport);

}  // namespace flare::write_mostly
