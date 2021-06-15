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

#include "flare/rpc/internal/rpc_metrics.h"

#include "benchmark/benchmark.h"

#include "flare/testing/echo_service.flare.pb.h"

// Run on (76 X 2494.14 MHz CPU s)
// CPU Caches:
//   L1 Data 32K (x76)
//   L1 Instruction 32K (x76)
//   L2 Unified 4096K (x76)
// Load Average: 2.19, 26.49, 23.58
// ---------------------------------------------------------------------
// Benchmark                           Time             CPU   Iterations
// ---------------------------------------------------------------------
// Benchmark_RpcMetricsReport       32.1 ns         32.1 ns     21837211

namespace flare::rpc::detail {

void Benchmark_RpcMetricsReport(benchmark::State& state) {
  const google::protobuf::ServiceDescriptor* service =
      testing::EchoService::descriptor();
  while (state.KeepRunning()) {
    RpcMetrics::Instance()->Report(service->method(0), 0, 10, 1234567, 1234567);
  }
}

BENCHMARK(Benchmark_RpcMetricsReport);

}  // namespace flare::rpc::detail
