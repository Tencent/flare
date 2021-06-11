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

#include "flare/rpc/protocol/http/buffer_io.h"

#include "benchmark/benchmark.h"

#include "flare/base/logging.h"
#include "flare/net/http/http_headers.h"
#include "flare/net/http/http_request.h"

// Run on (76 X 2494.14 MHz CPU s)
// CPU Caches:
//   L1 Data 32K (x76)
//   L1 Instruction 32K (x76)
//   L2 Unified 4096K (x76)
// Load Average: 0.33, 0.23, 0.35
// -----------------------------------------------------------------
// Benchmark                       Time             CPU   Iterations
// -----------------------------------------------------------------
// Benchmark_ReadHeader1        77.1 ns         77.0 ns      9082340
// Benchmark_ReadHeader2         152 ns          151 ns      4630931
// Benchmark_WriteMessage       61.3 ns         61.0 ns     11468376

namespace flare::http {

// Got from `curl http://www.baidu.com`.
constexpr auto kHeader1 =
    "GET / HTTP/1.1\r\n"
    "User-Agent: curl/7.29.0\r\n"
    "Host: www.baidu.com\r\n"
    "Accept: */*\r\n"
    "\r\n";

constexpr auto kHeader2 =
    "HTTP/1.1 200 OK\r\n"
    "Accept-Ranges: bytes\r\n"
    "Cache-Control: private, no-cache, no-store, proxy-revalidate, "
    "no-transform\r\n"
    "Connection: keep-alive\r\n"
    "Content-Length: 2443\r\n"
    "Content-Type: text/html\r\n"
    "Date: Mon, 30 Mar 2020 11:17:19 GMT\r\n"
    "Etag: \"58860402-98b\"\r\n"
    "Last-Modified: Mon, 23 Jan 2017 13:24:18 GMT\r\n"
    "Pragma: no-cache\r\n"
    "Server: bfe/1.0.8.18\r\n"
    "Set-Cookie: BDORZ=27315; max-age=86400; domain=.baidu.com; path=/\r\n"
    "\r\n";

void Benchmark_ReadHeader1(benchmark::State& state) {
  auto buffer = CreateBufferSlow(kHeader1);
  while (state.KeepRunning()) {
    std::pair<std::unique_ptr<char[]>, std::size_t> read;
    FLARE_CHECK(ReadHeader(buffer, &read) == ReadStatus::OK);
  }
}

BENCHMARK(Benchmark_ReadHeader1);

void Benchmark_ReadHeader2(benchmark::State& state) {
  auto buffer = CreateBufferSlow(kHeader2);
  while (state.KeepRunning()) {
    std::pair<std::unique_ptr<char[]>, std::size_t> read;
    FLARE_CHECK(ReadHeader(buffer, &read) == ReadStatus::OK);
  }
}

BENCHMARK(Benchmark_ReadHeader2);

void Benchmark_WriteMessage(benchmark::State& state) {
  auto buffer = CreateBufferSlow(kHeader1);
  std::pair<std::unique_ptr<char[]>, std::size_t> read;
  FLARE_CHECK(ReadHeader(buffer, &read) == ReadStatus::OK);
  HttpRequest request;
  std::string_view start_line;
  FLARE_CHECK(
      ParseMessagePartial(std::move(read), &start_line, request.headers()));
  request.set_method(HttpMethod::Post);
  request.set_uri("/path/to/something");
  while (state.KeepRunning()) {
    NoncontiguousBufferBuilder builder;
    WriteMessage(request, &builder);
    benchmark::DoNotOptimize(builder.DestructiveGet());
  }
}

BENCHMARK(Benchmark_WriteMessage);

// TODO(luobogao): ParseMessagePartial

}  // namespace flare::http
