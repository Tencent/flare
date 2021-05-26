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

#include "flare/base/monitoring.h"

#include "thirdparty/benchmark/benchmark.h"

using namespace std::literals;

// Run on (76 X 2494.14 MHz CPU s)
// CPU Caches:
//   L1 Data 32K (x76)
//   L1 Instruction 32K (x76)
//   L2 Unified 4096K (x76)
// Load Average: 1.84, 5.67, 5.65
// --------------------------------------------------------------------
// Benchmark                          Time             CPU   Iterations
// --------------------------------------------------------------------
// Benchmark_Report                6.17 ns         6.16 ns    113411048
// Benchmark_Counter               2.55 ns         2.54 ns    275147513
// Benchmark_CounterTagged         36.0 ns         36.0 ns     19465379
// Benchmark_TimerFast             3.62 ns         3.61 ns    193701641
// Benchmark_TimerSlow             14.9 ns         14.9 ns     47138624
// Benchmark_Timer8MsFast          12.2 ns         12.2 ns     57155006
// Benchmark_TimerTaggedFast       36.4 ns         36.3 ns     19287305
// Benchmark_TimerTaggedSlow       51.1 ns         51.1 ns     13724080

namespace flare::monitoring {

MonitoredCounter counter("attr");
MonitoredTimer timer("timer", 1ms);
MonitoredTimer timer2("timer", 10ms);
MonitoredTimer timer_tagged("timer2", 1ms);

// This benchmark result makes little sense as a majority of reported events are
// silently dropped due to internal event queue full.
//
// However it does help in inspecting assembly produced by the compiler.
void Benchmark_Report(benchmark::State& state) {
  // `NullMonitoringSystem` is used by default. It satisfies our need perfectly.
  while (state.KeepRunning()) {
    Report("my fancy key"sv, 12345);
  }
}

BENCHMARK(Benchmark_Report);

void Benchmark_Counter(benchmark::State& state) {
  while (state.KeepRunning()) {
    counter.Add(5);
  }
}

BENCHMARK(Benchmark_Counter);

void Benchmark_CounterTagged(benchmark::State& state) {
  while (state.KeepRunning()) {
    counter.Add(5, {{"key", "value"}});
  }
}

BENCHMARK(Benchmark_CounterTagged);

void Benchmark_TimerFast(benchmark::State& state) {
  int i = 0;
  while (state.KeepRunning()) {
    timer.Report(1ms * ((i++) % 64));
  }
}

BENCHMARK(Benchmark_TimerFast);

void Benchmark_TimerSlow(benchmark::State& state) {
  int i = 0;
  while (state.KeepRunning()) {
    timer.Report(1s * ((i++) % 16384));
  }
}

BENCHMARK(Benchmark_TimerSlow);

void Benchmark_Timer8MsFast(benchmark::State& state) {
  int i = 0;
  while (state.KeepRunning()) {
    timer2.Report(1ms * ((i++) % 64));
  }
}

BENCHMARK(Benchmark_Timer8MsFast);

void Benchmark_TimerTaggedFast(benchmark::State& state) {
  int i = 0;
  while (state.KeepRunning()) {
    timer_tagged.Report(1ms * ((i++) % 64), {{"tag", "value"}});
  }
}

BENCHMARK(Benchmark_TimerTaggedFast);

void Benchmark_TimerTaggedSlow(benchmark::State& state) {
  int i = 0;
  while (state.KeepRunning()) {
    timer_tagged.Report(1s * ((i++) % 16384), {{"tag", "value"}});
  }
}

BENCHMARK(Benchmark_TimerTaggedSlow);

}  // namespace flare::monitoring
