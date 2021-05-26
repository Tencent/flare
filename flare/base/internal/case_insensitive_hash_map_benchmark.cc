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

#include "flare/base/internal/case_insensitive_hash_map.h"

#include <chrono>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "thirdparty/benchmark/benchmark.h"

#include "flare/base/random.h"

namespace flare::internal {

constexpr auto kMaxKeysCount = 1048576;
std::vector<std::string> keys_to_insert = [] {
  std::vector<std::string> strs;
  for (int i = 0; i != kMaxKeysCount; ++i) {
    strs.push_back("asdfasfas" + std::to_string(Random() * 12345678ULL));
  }
  return strs;
}();
std::vector<std::string_view> keys_to_find = [] {
  std::vector<std::string_view> svs;
  for (auto&& e : keys_to_insert) {
    svs.push_back(e);
  }
  return svs;
}();
std::vector<std::string> keys_to_find_404 = [] {
  auto v = keys_to_insert;
  for (auto&& e : v) {
    e += "_404";
  }
  return v;
}();

void Benchmark_MapInsert(benchmark::State& state) {
  std::map<std::string_view, std::string_view> m;
  int x = 0;
  while (state.KeepRunning()) {
    m[keys_to_insert[x % state.range(0)]] = "something not very meaningful";
    ++x;
  }
}

BENCHMARK(Benchmark_MapInsert)->Range(4, 8192);

void Benchmark_UnorderedMapInsert(benchmark::State& state) {
  std::unordered_map<std::string_view, std::string_view> m;
  int x = 0;
  while (state.KeepRunning()) {
    m[keys_to_insert[x % state.range(0)]] = "something not very meaningful";
    ++x;
  }
}

BENCHMARK(Benchmark_UnorderedMapInsert)->Range(4, 8192);

void Benchmark_HashMapInsert(benchmark::State& state) {
  HashMap<std::string_view, std::string_view> m;
  int x = 0;
  while (state.KeepRunning()) {
    m[keys_to_insert[x % state.range(0)]] = "something not very meaningful";
    ++x;
  }
}

BENCHMARK(Benchmark_HashMapInsert)->Range(4, 8192);

void Benchmark_CaseInsensitiveHashMapInsert(benchmark::State& state) {
  CaseInsensitiveHashMap<std::string_view, std::string_view> m;
  int x = 0;
  while (state.KeepRunning()) {
    m[keys_to_insert[x % state.range(0)]] = "something not very meaningful";
    ++x;
  }
}

BENCHMARK(Benchmark_CaseInsensitiveHashMapInsert)->Range(4, 8192);

void Benchmark_MapFind(benchmark::State& state) {
  std::map<std::string_view, std::string_view> m;
  for (int i = 0; i != state.range(0); ++i) {
    m[keys_to_insert[i % kMaxKeysCount]] = "something not very meaningful";
  }
  int x = 0;
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(m.find(keys_to_find[x % state.range(0)]));
    ++x;
  }
}

BENCHMARK(Benchmark_MapFind)->Range(4, 8192);

void Benchmark_UnorderedMapFind(benchmark::State& state) {
  std::unordered_map<std::string_view, std::string_view> m;
  for (int i = 0; i != state.range(0); ++i) {
    m[keys_to_insert[i % kMaxKeysCount]] = "something not very meaningful";
  }
  int x = 0;
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(m.find(keys_to_find[x % state.range(0)]));
    ++x;
  }
}

BENCHMARK(Benchmark_UnorderedMapFind)->Range(4, 8192);

void Benchmark_HashMapFind(benchmark::State& state) {
  HashMap<std::string_view, std::string_view> m;
  for (int i = 0; i != state.range(0); ++i) {
    m[keys_to_insert[i % kMaxKeysCount]] = "something not very meaningful";
  }
  int x = 0;
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(m.find(keys_to_find[x % state.range(0)]));
    ++x;
  }
}

BENCHMARK(Benchmark_HashMapFind)->Range(4, 8192);

void Benchmark_CaseInsensitiveHashMapFind(benchmark::State& state) {
  CaseInsensitiveHashMap<std::string_view, std::string_view> m;
  for (int i = 0; i != state.range(0); ++i) {
    m[keys_to_insert[i % kMaxKeysCount]] = "something not very meaningful";
  }
  int x = 0;
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(m.find(keys_to_find[x % state.range(0)]));
    ++x;
  }
}

BENCHMARK(Benchmark_CaseInsensitiveHashMapFind)->Range(4, 8192);

void Benchmark_CaseInsensitiveHashMapTryGet(benchmark::State& state) {
  CaseInsensitiveHashMap<std::string_view, std::string_view> m;
  for (int i = 0; i != state.range(0); ++i) {
    m[keys_to_insert[i % kMaxKeysCount]] = "something not very meaningful";
  }
  int x = 0;
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(m.find(keys_to_find[x % state.range(0)]));
    ++x;
  }
}

BENCHMARK(Benchmark_CaseInsensitiveHashMapTryGet)->Range(4, 8192);

void Benchmark_CaseInsensitiveHashMapTryGet404(benchmark::State& state) {
  CaseInsensitiveHashMap<std::string_view, std::string_view> m;
  for (int i = 0; i != state.range(0); ++i) {
    m[keys_to_insert[i % kMaxKeysCount]] = "something not very meaningful";
  }
  int x = 0;
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(m.TryGet(keys_to_find_404[x % state.range(0)]));
    ++x;
  }
}

BENCHMARK(Benchmark_CaseInsensitiveHashMapTryGet404)->Range(4, 8192);

}  // namespace flare::internal
