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

#include "flare/rpc/tracing/tracing_ops.h"

#include "opentracing/ext/tags.h"
#include "benchmark/benchmark.h"

#include "flare/rpc/tracing/framework_tags.h"

// Let's make sure the perf. overhead is minimal when tracing is not enabled.

// CPU Caches:
//   L1 Data 32K (x76)
//   L1 Instruction 32K (x76)
//   L2 Unified 4096K (x76)
// Load Average: 9.48, 13.33, 28.24
// -------------------------------------------------------------------------------
// Benchmark                                     Time             CPU Iterations
// -------------------------------------------------------------------------------
// Benchmark_TracingOpsStartSpan              2.18 ns         2.17 ns 323270537
// Benchmark_TracingOpsParseSpanContext       6.72 ns         6.72 ns 104242807
// Benchmark_QuickerSpan                      2.42 ns         2.42 ns 290771272

namespace flare::tracing {

TracingOps ops(nullptr);  // Usually shared globally, so construction overhead
                          // is not important.
std::string op_name = "sadf";
std::string remote_peer = "192.0.2.1";

void Benchmark_TracingOpsStartSpan(benchmark::State& state) {
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(
        ops.StartSpanWithLazyOptions(op_name, [](auto opts) {}));
  }
}

BENCHMARK(Benchmark_TracingOpsStartSpan);

void Benchmark_TracingOpsParseSpanContext(benchmark::State& state) {
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(ops.ParseSpanContextFrom(""));
  }
}

BENCHMARK(Benchmark_TracingOpsParseSpanContext);

void Benchmark_QuickerSpan(benchmark::State& state) {
  while (state.KeepRunning()) {
    auto span = ops.StartSpanWithLazyOptions(op_name, [](auto opts) {});
    span.SetStandardTag(opentracing::ext::peer_host_ipv4, remote_peer);
    span.SetStandardTag(opentracing::ext::peer_port, 12345);
    span.SetFrameworkTag(ext::kTrackingId, "123");
    span.SetUserTag("user.tag1", "name");
    span.Report();
    benchmark::DoNotOptimize(span);
  }
}

BENCHMARK(Benchmark_QuickerSpan);

}  // namespace flare::tracing
