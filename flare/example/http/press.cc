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

#include <atomic>

#include "flare/base/thread/latch.h"
#include "flare/fiber/async.h"
#include "flare/fiber/runtime.h"
#include "flare/fiber/this_fiber.h"
#include "flare/init.h"
#include "flare/init/override_flag.h"
#include "flare/net/http/http_client.h"
#include "flare/rpc/server.h"
#include "flare/rpc/server_group.h"

using namespace std::literals;

DEFINE_string(url, "", "Http request url.");
DEFINE_int32(max_pending, 20000, "Maximum number of on-fly requests.");
DEFINE_string(body, "123", "Http Post body.");
DEFINE_int32(timeout, 1000, "Timeout for requests, in milliseconds.");
DEFINE_int32(
    dummy_server_port, 0,
    "If set to non-zero, a dummy server is restart at the given port. This can "
    "help you in inspecting the framework actions (via /inspect/vars).");

FLARE_OVERRIDE_FLAG(logtostderr, true);
FLARE_OVERRIDE_FLAG(flare_fiber_scheduling_optimize_for, "io-heavy");

namespace example::http_echo {

std::atomic<std::size_t> pending_requests_{};

// counter[timeout in microseconds] = number of requests
std::atomic<std::size_t> counter[2000000];

std::atomic<bool> stopping{false};

struct CallContext {
  flare::HttpClient client;
  flare::HttpRequest req;
};

void AsyncCall(std::shared_ptr<CallContext> call_ctx) {
  ++pending_requests_;
  auto start = flare::ReadSteadyClock();
  flare::HttpClient::RequestOptions opts{
      .timeout = FLAGS_timeout * 1ms,
      .content_type = "text/html",
  };
  call_ctx->client.AsyncPost(FLAGS_url, FLAGS_body, opts, nullptr)
      .Then([=](const flare::Expected<flare::HttpResponse,
                                      flare::HttpClient::ErrorCode>& e) {
        int time_used = (flare::ReadSteadyClock() - start) / 1us;
        if (!e) {
          FLARE_LOG_ERROR_EVERY_SECOND(
              "not succ {} {}", flare::HttpClient::ErrorCodeToString(e.error()),
              time_used / 1000);
        }
        time_used = std::min<std::size_t>(time_used, std::size(counter) - 1);
        ++counter[time_used];
        if (!stopping.load(std::memory_order_relaxed)) {
          AsyncCall(call_ctx);
        }
        --pending_requests_;
      });
}

void GenerateWorkload() { AsyncCall(std::make_shared<CallContext>()); }

void DumpStatistics() {
  thread_local std::vector<std::size_t> local_counter(std::size(counter));
  memcpy(local_counter.data(), counter, sizeof(counter));
  memset(reinterpret_cast<void*>(counter), 0, sizeof(counter));

  std::uint64_t avg = 0;
  for (int i = 0; i != local_counter.size(); ++i) {
    avg += local_counter[i] * i;
    if (i) {
      local_counter[i] += local_counter[i - 1];
    }
  }

#define UPDATE_STATISTIC(var, x, y)              \
  if (!var && local_counter[i] > reqs * x / y) { \
    var = i;                                     \
  }

  std::uint64_t p90 = 0, p95 = 0, p99 = 0, p995 = 0, p999 = 0, p9999 = 0,
                max = 0;
  std::size_t reqs = local_counter.back();
  avg /= reqs;
  for (int i = 0; i != std::size(local_counter); ++i) {
    UPDATE_STATISTIC(p90, 90, 100);
    UPDATE_STATISTIC(p95, 95, 100);
    UPDATE_STATISTIC(p99, 99, 100);
    UPDATE_STATISTIC(p995, 995, 1000);
    UPDATE_STATISTIC(p999, 999, 1000);
    UPDATE_STATISTIC(p9999, 9999, 10000);
    if (local_counter[i] == reqs) {
      max = i;
      break;
    }
  }
  FLARE_LOG_INFO(
      "avg: {}us, p90: {}us, p95: {}us, p99: {}us, p995: {}us, p999: {}us, "
      "p9999: {}us, max: {}us.",
      avg, p90, p95, p99, p995, p999, p9999, max);
}

int Entry(int argc, char** argv) {
  flare::ServerGroup servers;
  if (FLAGS_dummy_server_port) {
    auto&& server = *servers.AddServer();
    server.ListenOn(
        flare::EndpointFromIpv4("0.0.0.0", FLAGS_dummy_server_port));
    server.AddProtocol("http");
  }
  servers.Start();

  for (int i = 0; i != FLAGS_max_pending; ++i) {
    flare::fiber::Async(flare::fiber::Launch::Post,
                        i % flare::fiber::GetSchedulingGroupCount(),
                        GenerateWorkload);
    if (FLAGS_max_pending < 1000 || i % (FLAGS_max_pending / 1000) == 0) {
      flare::this_fiber::SleepFor(1ms);
    }
  }

  while (true) {
    flare::this_fiber::SleepFor(10s);
    DumpStatistics();
  }

  servers.Stop();
  servers.Join();
  return 0;
}

}  // namespace example::http_echo

int main(int argc, char** argv) {
  return flare::Start(argc, argv, example::http_echo::Entry);
}
