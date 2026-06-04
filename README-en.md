# Flare Backend Service Framework

[中文文档](README.md)

[![license NewBSD](https://img.shields.io/badge/license-BSD-yellow.svg)](LICENSE)
[![C++](https://img.shields.io/badge/language-C%2B%2B20-blue.svg)](https://isocpp.org/)
[![Code Style](https://img.shields.io/badge/code%20style-google-blue.svg)](https://google.github.io/styleguide/cppguide.html)
[![Platform](https://img.shields.io/badge/platform-linux%20%7C%20macOS-lightgrey.svg)](#system-requirements)

[Tencent Ads](https://e.qq.com/ads/) is one of the most important businesses at Tencent, with a backend heavily developed in C++.

Flare is a modern backend service development framework, built on the experience of our in-house service frameworks, popular open-source projects, and the latest research. It aims to provide easy-to-use, high-performance, and stable service-development capabilities for today's mainstream software and hardware environments.

The Flare project started in 2019 and is now widely used across Tencent Ads' backend services, with tens of thousands of running instances battle-tested in real production systems.

In May 2021, Flare was officially open-sourced, in the spirit of giving back to the community and sharing what we've learned.

## Features

- Modern C++ style — full adoption of C++20 syntax features and the C++20 standard library.
- An [M:N thread model](https://en.wikipedia.org/wiki/Thread_(computing)) user-space [Fiber](flare/doc/fiber.md) implementation, so business code can be written with synchronous syntax while keeping asynchronous performance.
- Message-based [streaming RPC](flare/doc/streaming-rpc.md).
- In addition to RPC, a set of convenient [base libraries](flare/base): strings, time and date, encoding, compression, encryption, configuration, an HTTP client, and more — designed to get you productive quickly.
- A flexible extension mechanism for additional protocols, service discovery, load balancing, monitoring and alerting, [RPC tracing](flare/doc/tracing.md), and more.
- Extensive optimizations for modern hardware: [NUMA-aware](https://en.wikipedia.org/wiki/Non-uniform_memory_access) [scheduling groups](flare/doc/scheduling-group.md), [object pools](flare/doc/object-pool.md), [zero-copy buffers](flare/doc/buffer.md), etc.
- High-quality code. Strictly follows the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html), with around 80% test coverage.
- Comprehensive [documentation](flare/doc), [examples](flare/example), and [debugging support](flare/doc/debugging.md), so you can get started quickly.

## System Requirements

### Linux

- Linux kernel 3.10 or later.
- GCC 11 or later — this is the first libstdc++ version with `std::atomic<T>::wait` / `notify_*` (see [P1135](https://wg21.link/p1135)).
- x86-64 processors. aarch64 and ppc64le are also supported but have not been used in production.

### macOS

- macOS 13 (Ventura) or later.
- Apple Clang — Xcode 14+ ships a libc++ with the runtime symbols for `std::atomic<T>::wait` / `notify_*`.
- Apple Silicon (arm64) and Intel (x86_64) are both supported.

## Getting Started

Flare is "batteries included" — all required [third-party libraries](thirdparty/) ship in-tree, so no additional dependency installation is normally needed.

Source tarballs under `thirdparty/` are stored via [Git LFS](https://git-lfs.github.com/), so make sure `git-lfs` is properly installed before cloning.

### Building

We use [`blade`](https://github.com/blade-build/blade-build) for day-to-day development:

- Compile: `./blade build ...`
- Test: `./blade test ...`

Flare also supports [`bazel`](https://bazel.build) as a build system — see [bazel support](bazel/support/README.md).

After that, follow the [Getting Started Guide](flare/doc/intro-rpc.md) to bring up a simple RPC service.

### Debugging

We treat [debugging experience](flare/doc/debugging.md) as a first-class concern of day-to-day maintenance, and ship the following out of the box:

- [A GDB plugin that lists suspended fibers](flare/doc/gdb-plugin.md)
- [Support for various Sanitizers](flare/doc/sanitizers.md)

### Testing

To improve the experience of writing unit tests, we provide a set of [testing utilities](flare/doc/testing.md), including but not limited to:

- [RPC Mock](flare/testing/rpc_mock.h)
- [Redis Mock](flare/testing/redis_mock.h)
- [COS Mock](flare/testing/cos_mock.h)
- [HTTP Mock](flare/testing/http_mock.h)
- [Non-virtual function Mock](flare/testing/hooking_mock.h)
- And several helper utilities.

### Example

We've put together several [examples](flare/example) for reference. The snippet below is a simple relay service that exercises both the RPC client and server sides:

```cpp
#include "gflags/gflags.h"

#include "flare/example/rpc/echo_service.flare.pb.h"
#include "flare/example/rpc/relay_service.flare.pb.h"
#include "flare/fiber/this_fiber.h"
#include "flare/init.h"
#include "flare/rpc/rpc_channel.h"
#include "flare/rpc/rpc_client_controller.h"
#include "flare/rpc/rpc_server_controller.h"
#include "flare/rpc/server.h"

using namespace std::literals;

DEFINE_string(ip, "127.0.0.1", "IP address to listen on.");
DEFINE_int32(port, 5569, "Port to listen on.");
DEFINE_string(forward_to, "flare://127.0.0.1:5567",
              "Target IP to forward requests to.");

namespace example {

class RelayServiceImpl : public SyncRelayService {
 public:
  void Relay(const RelayRequest& request, RelayResponse* response,
             flare::RpcServerController* ctlr) override {
    flare::RpcClientController our_ctlr;
    EchoRequest echo_req;
    echo_req.set_body(request.body());
    if (auto result = stub_.Echo(echo_req, &our_ctlr)) {
      response->set_body(result->body());
    } else {
      ctlr->SetFailed(result.error().code(), result.error().message());
    }
  }

 private:
  EchoService_SyncStub stub_{FLAGS_forward_to};
};

int Entry(int argc, char** argv) {
  flare::Server server{flare::Server::Options{.service_name = "relay_server"}};

  server.AddProtocol("flare");
  server.AddService(std::make_unique<RelayServiceImpl>());
  server.ListenOn(flare::EndpointFromIpv4(FLAGS_ip, FLAGS_port));
  FLARE_CHECK(server.Start());

  flare::WaitForQuitSignal();
  return 0;
}

}  // namespace example

int main(int argc, char** argv) {
  return flare::Start(argc, argv, example::Entry);
}
```

Flare is built on top of [M:N user-space threads](flare/doc/fiber.md), so synchronous requests to external services through Flare — and the synchronous interfaces of [Flare's built-in clients](flare/net) — don't carry a performance penalty. For more complex concurrent or asynchronous needs, see the [Fiber documentation](flare/doc/fiber.md).

Note also that the `*.flare.pb.h` in the example is generated by [Flare's Protocol Buffers plugin](flare/doc/pb-plugin.md). Compared to Protocol Buffers' own `cc_generic_services`, the resulting interfaces are easier to use.

## A More Complex Example

In real use, you'll often want to fan out concurrent requests to multiple backends. The example below shows how to do that with Flare:

```cpp
// For illustration purpose only. Normally you wouldn't want to declare them as
// global variables.
flare::HttpClient http_client;
flare::CosClient cos_client;
EchoService_SyncStub echo_stub(FLAGS_echo_server_addr);

void FancyServiceImpl::FancyJob(const FancyJobRequest& request,
                                FancyJobResponse* response,
                                flare::RpcServerController* ctlr) {
  // Calling different services concurrently.
  auto async_http_body = http_client.AsyncGet(request.uri());
  auto async_cos_data =
      cos_client.AsyncExecute(flare::CosGetObjectRequest{.key = request.key()});
  EchoRequest echo_req;
  flare::RpcClientController echo_ctlr;
  echo_req.set_body(request.echo());
  auto async_rpc_resp = echo_stub.AsyncEcho(EchoRequest(), &echo_ctlr);

  // Now wait for all of them to complete.
  auto&& [http, cos, rpc] = flare::fiber::BlockingGet(
      flare::WhenAll(&async_http_body, &async_cos_data, &async_rpc_resp));

  if (!http || !cos || !rpc) {
    FLARE_LOG_WARNING("Failed.");
  } else {
    // All succeeded.
    FLARE_LOG_INFO("Got: {}, {}, {}", *http->body(),
                   flare::FlattenSlow(cos->bytes), rpc->body());
  }

  // Now fill `response` accordingly.
  response->set_body("Great success.");
}
```

In this example we:

- Issue three asynchronous requests through three different clients ([HTTP](flare/net/http/http_client.h), [Tencent Cloud COS](flare/net/cos/cos_client.h), and RPC).
- Wait for all of them with `flare::fiber::BlockingGet` — this only blocks the user-space thread, so there's no performance issue.
- Log the response from each service.

For demonstration purposes we requested three heterogeneous services here, but you can use the same pattern for homogeneous, or partially homogeneous and partially heterogeneous, backends.

## Contributing

Contributions are very welcome. Developers interested in Flare's internals, or those who want to fork and extend it, will find more technical documentation under [`flare/doc/`](flare/doc/).

See [CONTRIBUTING.md](CONTRIBUTING.md) for details.

## Performance

Because of the nature of our workload, our design leans towards optimizing latency and its stability rather than raw throughput — while still keeping performance reasonable under that constraint.

For a simple side-by-side comparison, see the [preliminary performance numbers](flare/doc/benchmark.md).

## Credits

- Our lower-level implementation draws heavily on the design of [brpc](https://github.com/apache/incubator-brpc).
- The RPC layer was inspired by [grpc](https://grpc.io/) in many places.
- We rely on a number of [third-party libraries](thirdparty/) from the open-source community. Standing on the shoulders of giants lets us build this project faster and better — and is why we are happy to give back.

Our sincere thanks to all of the above.
