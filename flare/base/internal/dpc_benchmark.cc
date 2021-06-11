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

#include "flare/base/internal/dpc.h"

#include <chrono>

#include "benchmark/benchmark.h"

#include "flare/base/internal/background_task_host.h"
#include "flare/init.h"

using namespace std::literals;

namespace flare::internal {

void Benchmark_QueueDpc(benchmark::State& state) {
  for (int i = 0; i != 10000000; ++i) {
    QueueDpc([] {});
  }
  while (state.KeepRunning()) {
    QueueDpc([] {});
  }
}

BENCHMARK(Benchmark_QueueDpc);

}  // namespace flare::internal

int main(int argc, char** argv) {
  ::benchmark::Initialize(&argc, argv);
  if (::benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
  flare::InitializeBasicRuntime();
  ::benchmark::RunSpecifiedBenchmarks();
}
