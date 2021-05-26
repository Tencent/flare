// Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "flare/base/callback.h"
#include "flare/example/rpc/echo_service.flare.pb.h"
#include "flare/example/rpc/relay_service.pb.h"
#include "flare/fiber/async.h"
#include "flare/fiber/runtime.h"
#include "flare/fiber/this_fiber.h"
#include "flare/init.h"
#include "flare/init/override_flag.h"
#include "flare/rpc/rpc_channel.h"
#include "flare/rpc/rpc_client_controller.h"
#include "flare/rpc/server.h"
#include "flare/rpc/server_group.h"

using namespace std::literals;

DEFINE_string(server_addr, "", "Server address.");
DEFINE_int32(max_pending, 20000, "Maximum number of on-fly requests.");
DEFINE_int32(timeout, 1000, "Timeout for requests, in milliseconds.");
DEFINE_bool(relay_stub, false,
            "If you're pressing relay_server, enable this flag.");
DEFINE_string(override_nslb, "",
              "Override default NSLB for scheme used in `server_addr");
DEFINE_int32(
    dummy_server_port, 0,
    "If set to non-zero, a dummy server is restart at the given port. This can "
    "help you in inspecting the framework actions (via /inspect/vars).");
DEFINE_int32(
    attachment_size, 0,
    "If non-zero, an attachment of such size is sent along with the request.");
DEFINE_string(body, "", "If non-empty, specifies body of echo request.");
DEFINE_string(
    compression_algorithm, "",
    "If non-empty, specifies compression algorithm used for compressing "
    "request. Choices are: `lz4-frame`, `snappy`, `gzip`.");

FLARE_OVERRIDE_FLAG(logbufsecs, 0);
FLARE_OVERRIDE_FLAG(logtostderr, true);
FLARE_OVERRIDE_FLAG(flare_fiber_scheduling_optimize_for, "io-heavy");

namespace example::protobuf_echo {

std::atomic<std::size_t> pending_requests_{};

// counter[timeout in microseconds] = number of requests
std::atomic<std::size_t> counter[2000000];

std::atomic<bool> stopping{false};

struct CallContext {
  flare::RpcChannel channel;
  flare::RpcClientController ctlr;

  std::unique_ptr<EchoService_Stub> stub;
  EchoRequest req;
  EchoResponse resp;

  std::unique_ptr<RelayService_Stub> relay_stub;
  RelayRequest relay_req;
  RelayResponse relay_resp;
};

const flare::NoncontiguousBuffer& GetAttachment() {
  thread_local auto attach =
      flare::CreateBufferSlow(std::string(FLAGS_attachment_size, 'a'));
  return attach;
}

flare::rpc::CompressionAlgorithm GetCompressionAlgorithm() {
  if (FLAGS_compression_algorithm.empty()) {
    return flare::rpc::COMPRESSION_ALGORITHM_NONE;
  } else if (FLAGS_compression_algorithm == "lz4-frame") {
    return flare::rpc::COMPRESSION_ALGORITHM_LZ4_FRAME;
  } else if (FLAGS_compression_algorithm == "snappy") {
    return flare::rpc::COMPRESSION_ALGORITHM_SNAPPY;
  } else if (FLAGS_compression_algorithm == "gzip") {
    return flare::rpc::COMPRESSION_ALGORITHM_GZIP;
  } else if (FLAGS_compression_algorithm == "zstd") {
    return flare::rpc::COMPRESSION_ALGORITHM_ZSTD;
  }
  FLARE_LOG_FATAL("Unrecognized compression algorithm: {}",
                  FLAGS_compression_algorithm);
}

void AsyncCall(std::shared_ptr<CallContext> call_ctx) {
  ++pending_requests_;
  auto start = flare::ReadSteadyClock();
  auto cb = flare::NewCallback([=] {
    int time_used = (flare::ReadSteadyClock() - start) / 1us;
    time_used = std::min<std::size_t>(time_used, std::size(counter) - 1);
    ++counter[time_used];
    if (!stopping.load(std::memory_order_relaxed)) {
      call_ctx->ctlr.Reset();
      call_ctx->ctlr.SetRequestAttachment(GetAttachment());
      call_ctx->ctlr.SetTimeout(flare::ReadSteadyClock() + 1ms * FLAGS_timeout);
      AsyncCall(call_ctx);
    }
    --pending_requests_;
  });
  if (FLAGS_relay_stub) {
    call_ctx->relay_stub->Relay(&call_ctx->ctlr, &call_ctx->relay_req,
                                &call_ctx->relay_resp, cb);
  } else {
    call_ctx->stub->Echo(&call_ctx->ctlr, &call_ctx->req, &call_ctx->resp, cb);
  }
}

void GenerateWorkload() {
  auto ctx = std::make_shared<CallContext>();
  FLARE_CHECK(ctx->channel.Open(
      FLAGS_server_addr,
      flare::RpcChannel::Options{.override_nslb = FLAGS_override_nslb}));
  ctx->ctlr.SetTimeout(flare::ReadSteadyClock() + 1ms * FLAGS_timeout);
  ctx->ctlr.SetCompressionAlgorithm(GetCompressionAlgorithm());
  if (FLAGS_relay_stub) {
    ctx->relay_req.set_body(FLAGS_body);
    ctx->relay_stub = std::make_unique<RelayService_Stub>(&ctx->channel);
  } else {
    ctx->req.set_body(FLAGS_body);
    ctx->stub = std::make_unique<EchoService_Stub>(&ctx->channel);
  }
  AsyncCall(ctx);
}

void DumpStatistics() {
  thread_local std::vector<std::size_t> local_counter(std::size(counter));
  memcpy(local_counter.data(), counter, sizeof(counter));
  memset(reinterpret_cast<void*>(counter), 0, sizeof(counter));

  std::uint64_t total_time_usage = 0;
  for (int i = 0; i != local_counter.size(); ++i) {
    total_time_usage += local_counter[i] * i;
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
  std::uint64_t avg;
  if (reqs) {
    avg = total_time_usage / reqs;
  } else {
    avg = 0;
  }
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

int Entry(int, char**) {
  flare::ServerGroup servers;

  if (FLAGS_dummy_server_port) {
    auto&& server = *servers.AddServer();
    server.ListenOn(
        flare::EndpointFromIpv4("127.0.0.1", FLAGS_dummy_server_port));
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

}  // namespace example::protobuf_echo

int main(int argc, char** argv) {
  return flare::Start(argc, argv, example::protobuf_echo::Entry);
}
