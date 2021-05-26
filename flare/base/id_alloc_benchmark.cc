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

#include "flare/base/id_alloc.h"

#include "thirdparty/benchmark/benchmark.h"

namespace flare {

struct DummyTraits {
  using Type = std::uint32_t;
  static constexpr auto kMin = 1, kMax = 10000;
  static constexpr auto kBatchSize = 128;
};

void Benchmark_Next(benchmark::State& state) {
  while (state.KeepRunning()) {
    id_alloc::Next<DummyTraits>();
  }
}

BENCHMARK(Benchmark_Next);

}  // namespace flare
