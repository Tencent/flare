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

#include "flare/base/internal/hash_map.h"

#include <chrono>
#include <map>
#include <unordered_map>

#include "thirdparty/benchmark/benchmark.h"

// Run on (76 X 2494.14 MHz CPU s)
// CPU Caches:
//   L1 Data 32K (x76)
//   L1 Instruction 32K (x76)
//   L2 Unified 4096K (x76)
// Load Average: 34.24, 39.16, 37.16
// -----------------------------------------------------------------------------
// Benchmark                                   Time             CPU   Iterations
// -----------------------------------------------------------------------------
// Benchmark_MapFind/4                      3.98 ns         3.98 ns    175440609
// Benchmark_MapFind/8                      5.06 ns         5.06 ns    138357098
// Benchmark_MapFind/64                     6.91 ns         6.91 ns    101448551
// Benchmark_MapFind/512                    8.04 ns         8.04 ns     86854930
// Benchmark_MapFind/4096                   17.9 ns         17.9 ns     39005996
// Benchmark_MapFind/8192                   24.2 ns         24.2 ns     29015314
// Benchmark_UnorderedMapFind/4             14.0 ns         14.0 ns     50131490
// Benchmark_UnorderedMapFind/8             13.5 ns         13.5 ns     51774277
// Benchmark_UnorderedMapFind/64            17.7 ns         17.7 ns     39695677
// Benchmark_UnorderedMapFind/512           15.9 ns         15.9 ns     44198262
// Benchmark_UnorderedMapFind/4096          10.4 ns         10.4 ns     67215508
// Benchmark_UnorderedMapFind/8192          10.4 ns         10.4 ns     67133747
// Benchmark_HashMapFind/4                  2.19 ns         2.19 ns    320278042
// Benchmark_HashMapFind/8                  2.30 ns         2.30 ns    304672122
// Benchmark_HashMapFind/64                 2.30 ns         2.30 ns    303271088
// Benchmark_HashMapFind/512                2.29 ns         2.28 ns    306175935
// Benchmark_HashMapFind/4096               2.17 ns         2.17 ns    322581843
// Benchmark_HashMapFind/8192               2.17 ns         2.17 ns    322521003
// Benchmark_HashMapTryGet/4                1.89 ns         1.89 ns    369721579
// Benchmark_HashMapTryGet/8                2.21 ns         2.21 ns    317076948
// Benchmark_HashMapTryGet/64               2.61 ns         2.61 ns    268997446
// Benchmark_HashMapTryGet/512              2.17 ns         2.16 ns    326264717
// Benchmark_HashMapTryGet/4096             1.89 ns         1.89 ns    371350291
// Benchmark_HashMapTryGet/8192             1.89 ns         1.88 ns    370989098
// Benchmark_HashMapTryGet404               1.79 ns         1.79 ns    392514308
// Benchmark_HashMapTryGetPtr/4             2.10 ns         2.10 ns    335958636
// Benchmark_HashMapTryGetPtr/8             2.66 ns         2.66 ns    263659371
// Benchmark_HashMapTryGetPtr/64            2.42 ns         2.42 ns    289101861
// Benchmark_HashMapTryGetPtr/512           1.72 ns         1.72 ns    406768628
// Benchmark_HashMapTryGetPtr/4096          1.73 ns         1.73 ns    403808302
// Benchmark_HashMapTryGetPtr/8192          1.73 ns         1.73 ns    404032421
// Benchmark_MapString/4                    26.3 ns         26.3 ns     26580468
// Benchmark_MapString/8                    31.9 ns         31.9 ns     21959129
// Benchmark_MapString/64                   55.7 ns         55.6 ns     12526250
// Benchmark_MapString/512                   106 ns          106 ns      6588878
// Benchmark_MapString/4096                  165 ns          165 ns      4243622
// Benchmark_MapString/8192                  181 ns          181 ns      3832884
// Benchmark_UnorderedMapString/4           24.8 ns         24.8 ns     28412151
// Benchmark_UnorderedMapString/8           23.7 ns         23.7 ns     29565605
// Benchmark_UnorderedMapString/64          27.8 ns         27.8 ns     25205434
// Benchmark_UnorderedMapString/512         33.1 ns         33.1 ns     21449029
// Benchmark_UnorderedMapString/4096        36.7 ns         36.6 ns     19339332
// Benchmark_UnorderedMapString/8192        36.6 ns         36.6 ns     19120772
// Benchmark_HashMapTryGetString/4          7.84 ns         7.84 ns     89044734
// Benchmark_HashMapTryGetString/8          8.42 ns         8.42 ns     83528477
// Benchmark_HashMapTryGetString/64         9.36 ns         9.35 ns     74328304
// Benchmark_HashMapTryGetString/512        15.8 ns         15.8 ns     44205562
// Benchmark_HashMapTryGetString/4096       14.8 ns         14.8 ns     46931032
// Benchmark_HashMapTryGetString/8192       15.2 ns         15.2 ns     45877654

namespace flare::internal {

void Benchmark_MapFind(benchmark::State& state) {
  std::map<std::uint64_t, std::string> m;
  for (int i = 0; i != state.range(0); ++i) {
    m[i] = std::to_string(i);
  }
  int x = 0;
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(m.find(x & 0xff));
    ++x;
  }
}

BENCHMARK(Benchmark_MapFind)->Range(4, 8192);

void Benchmark_UnorderedMapFind(benchmark::State& state) {
  std::unordered_map<std::uint64_t, std::string> m;
  for (int i = 0; i != state.range(0); ++i) {
    m[i] = std::to_string(i);
  }
  int x = 0;
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(m.find(x & 0xfff));
    ++x;
  }
}

BENCHMARK(Benchmark_UnorderedMapFind)->Range(4, 8192);

void Benchmark_HashMapFind(benchmark::State& state) {
  HashMap<std::uint64_t, std::string> m;
  for (int i = 0; i != state.range(0); ++i) {
    m[i] = std::to_string(i);
  }
  int x = 0;
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(m.find(x & 0xfff));
    ++x;
  }
}

BENCHMARK(Benchmark_HashMapFind)->Range(4, 8192);

void Benchmark_HashMapTryGet(benchmark::State& state) {
  HashMap<std::uint64_t, std::string> m;
  for (int i = 0; i != state.range(0); ++i) {
    m[i] = std::to_string(i);
  }
  int x = 0;
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(m.TryGet(x & 0xfff));
    ++x;
  }
}

BENCHMARK(Benchmark_HashMapTryGet)->Range(4, 8192);

void Benchmark_HashMapTryGet404(benchmark::State& state) {
  HashMap<std::uint64_t, std::string> m;
  for (int i = 0; i != 0x7f; ++i) {  // Half of lookups will fail.
    m[i] = std::to_string(i);
  }
  int x = 0;
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(m.TryGet(x & 0xff));
    ++x;
  }
}

BENCHMARK(Benchmark_HashMapTryGet404);

void Benchmark_HashMapTryGetPtr(benchmark::State& state) {
  HashMap<const int*, std::string> m;
  for (int i = 0; i != state.range(0); ++i) {
    m[reinterpret_cast<int*>(i * 8)] = std::to_string(i);
  }
  int x = 0;
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(m.TryGet(reinterpret_cast<int*>((x & 0xff) * 8)));
    ++x;
  }
}

BENCHMARK(Benchmark_HashMapTryGetPtr)->Range(4, 8192);

void Benchmark_MapString(benchmark::State& state) {
  std::vector<std::string> strs;
  for (int i = 0; i != 256; ++i) {
    strs.push_back(std::to_string(i * 12345678ULL));
  }
  std::map<std::string, std::string> m;
  for (int i = 0; i != state.range(0); ++i) {
    m[std::to_string(i * 12345678ULL)] = std::to_string(i);
  }
  int x = 0;
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(m.find(strs[x & 0xff]));
    ++x;
  }
}

BENCHMARK(Benchmark_MapString)->Range(4, 8192);

void Benchmark_UnorderedMapString(benchmark::State& state) {
  std::vector<std::string> strs;
  for (int i = 0; i != 256; ++i) {
    strs.push_back(std::to_string(i * 12345678ULL));
  }
  std::unordered_map<std::string, std::string> m;
  for (int i = 0; i != state.range(0); ++i) {
    m[std::to_string(i * 12345678ULL)] = std::to_string(i);
  }
  int x = 0;
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(m.find(strs[x & 0xff]));
    ++x;
  }
}

BENCHMARK(Benchmark_UnorderedMapString)->Range(4, 8192);

void Benchmark_HashMapTryGetString(benchmark::State& state) {
  std::vector<std::string> strs;
  for (int i = 0; i != 256; ++i) {
    strs.push_back(std::to_string(i * 12345678ULL));
  }
  HashMap<std::string, std::string> m;
  for (int i = 0; i != state.range(0); ++i) {
    m[std::to_string(i * 12345678ULL)] = std::to_string(i);
  }
  int x = 0;
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(m.TryGet(strs[x & 0xff]));
    ++x;
  }
}

BENCHMARK(Benchmark_HashMapTryGetString)->Range(4, 8192);

}  // namespace flare::internal
