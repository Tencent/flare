# Flare Backend Service Framework

[![license NewBSD](https://img.shields.io/badge/license-BSD-yellow.svg)](LICENSE)
[![C++](https://img.shields.io/badge/language-C++-blue.svg)](https://isocpp.org/)
[![Code Style](https://img.shields.io/badge/code%20style-google-blue.svg)](https://google.github.io/styleguide/cppguide.html)
[![Platform](https://img.shields.io/badge/platform-linux%20-lightgrey.svg)](https://www.kernel.org/)

[Tencent Ads](https://e.qq.com/ads/) is one of the most important businesses of Tencent, and its backend is heavily
developed in C++.

Flare is a modern back-end service development framework developed by us based on years of experience, popular open
source projects and the latest research results. It aims to provide easy-to-use, high-performance, and stable service
development capabilities in the current mainstream software and hardware environment.

The Flare project started in 2019 and is now widely used in many backend services of Tencent Ads, with tens of
thousands of running instances, and has been tested on real production systems.

In May 2021, Flare was officially open-sourced to the public to give back to the community and share technology.

More documents are being translated to English.

## Features

- Modern C++ design style with extensive adoption of new syntax features and standard libraries from C++11/14/17/2a
- Provides [M:N thread model](https://en.wikipedia.org/wiki/Thread_(computing)) micro-threaded implementation of
  [Fiber](flare/doc/fiber.md) for business developers to write high-performance asynchronous calls code with convenient
  synchronous calls syntax code
- Message based [streaming RPC](flare/doc/streaming-rpc.md) support
- In addition to RPC, a series of convenient [base libraries](flare/base) are provided, such as string, time and date,
  encoding processing, compression, encryption and decryption, configuration, HTTP client, etc., which are convenient to
  get started developing business code quickly
- Provides a flexible expansion mechanism. Easy to support multiple protocols, service discovery, load balancing,
  monitoring and alerting, [call tracking](flare/doc/), etc.
- Extensive optimizations for modern architectures. For example, the [scheduling group](flare/) and
  [object pool](flare/doc/object-pool.md) of [NUMA aware](https://en.wikipedia.org/wiki/Non-uniform_memory_access),
  [zero-copy buffer](flare /doc/buffer.md), etc.
- High-quality code. Strict adherence to [Google C++ Code Specification](https://google.github.io/styleguide/cppguide.html),
  with 80% test coverage
- Complete [documentation](flare/doc) and [examples](flare/example) and [debugging support](flare/doc/debugging.md),
  easy to get started quickly

## System Requirements

- Linux 3.10 kernel or above, no other OS supported at this time
- x86-64 processors, aarch64 and ppc64le are also supported, but have not been used in production environment
- GCC 8 or above compiler

## Getting Started

Flare comes out of the box with the required [third-party libraries](thirdparty/), so there is usually no need to install additional dependent libraries. Just pull the code and use it under Linux.

The tarball under `thirdparty/` were stored via [Git LFS](https://git-lfs.github.com/), so you need to make sure that `git-lfs` is properly installed before pulling the code.

### Building

We use [`blade`](https://github.com/chen3feng/blade-build) to build this project.

- Compile: `./blade build ...`.
- Test: `./blade test ...`.

After that, you can refer to the introduction in [Getting Started Guide](flare/doc/intro-rpc.md) to build a simple RPC service.

### Debugging

We believe that the [debugging](flare/doc/debugging.md) experience is also an important part of the development and maintenance process, and we have made some support for this as follows.

- [GDB plugin for listing pending fibers](flare/doc/gdb-plugin.md)
- [Support for various Sanitizers](flare/doc/sanitizers.md)

### Testing

To improve the experience of writing tests, we provide a number of tools for [writing tests](flare/doc/testing.md).

These include, but are not limited to:

- [RPC Mock](flare/testing/rpc_mock.h)
- [Redis Mock](flare/testing/redis_mock.h)
- [COS Mock](flare/testing/cos_mock.h)
- [HTTP Mock](flare/testing/http_mock.h)
- [Non-virtual Function Mock](flare/testing/hooking_mock.h)
- Some tool methods, etc.

### Example

We provided some [examples](flare/example) for reference, the following code snippet is a simple relay service (include both client and service).

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

With the builtin [M:N user mode thread](flare/doc/fiber.md) support, requesting outside services from flare with synchronous syntax and using the synchorous inteface of our [builtin  clients for many  popular protocols](flare/net) will not incur peforrmance penalty. For more complex concurrency or synchronous requirements, please refer the [document](flare/doc/fiber.md) for details.

Besides, the example `*.flare.pb.h` is generated by [our Protocol Buffers plugin](flare/doc/pb-plugin.md). The interfaces generated this way are easier to use compared to the ones generated by Protocol Buffers' `cc_generic_services`

## More Complex Examples

In practice you will often face scenarios where you need to request multiple backends concurrently, and the following example shows you how to do this with Flare.

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

In this example, we:

- Initiated three asynchronous requests via three different clients ([HTTP](flare/net/http/http_client.h), [Tencent Cloud COS](flare/net/cos/cos_client.h), RPC).
- Waiting for all requests to complete synchronously via `flare::fiber::BlockingGet`. Here we only block the user mode thread and there are no performance issues.
- Print logs to output the response of each service.

For demonstration purposes, we are requesting three heterogeneous services here. You can also request homogeneous, or partially homogeneous and partially heterogeneous services in this way if necessary.

## Contribution

For developers who want to learn more about the internal design of Flare, or who want to participate the development of Flare, more documentation can be found under under [`flare/doc/`](flare/doc/).

Please refer to [CONTRIBUTING.md](CONTRIBUTING.md) for more details.

## Performance

Due to the characteristics of the business requirements, we prefer to optimize the smoothness of latency and jitter rather than throughput in the design process, but also try to ensure performance under this premise.

For simple comparison purposes, we have provided [preliminary performance data](flare/doc/benchmark.md).

## Credits

- Our underlying implementation draws heavily on the design of [brpc](https://github.com/apache/incubator-brpc).
- For the RPC part, [grpc](https://grpc.io/) gave us a lot of inspiration.
- We rely on a lot of [third party libraries](thirdparty/) from the open source community, standing on the shoulders of giants allows us to develop this project faster and better, and therefore actively give back to the open source community.

We'd like to express our sincere gratitude and appreciation to them.
