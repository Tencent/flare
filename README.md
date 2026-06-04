# Flare 后台服务开发框架

Flare Backend Service Framework

[English Document](README-en.md)

[![license NewBSD](https://img.shields.io/badge/license-BSD-yellow.svg)](LICENSE)
[![C++](https://img.shields.io/badge/language-C%2B%2B20-blue.svg)](https://isocpp.org/)
[![Code Style](https://img.shields.io/badge/code%20style-google-blue.svg)](https://google.github.io/styleguide/cppguide.html)
[![Platform](https://img.shields.io/badge/platform-linux%20%7C%20macOS-lightgrey.svg)](#系统要求)

[腾讯广告](https://e.qq.com/ads/) 是腾讯公司最重要的业务之一，其后台大量采用 C++ 开发。

Flare 是我们在汲取自研服务框架经验、参考业界开源项目和最新研究成果的基础上，开发的现代化后台服务开发框架。目标是在当今主流软硬件环境下，提供易用、高性能、平稳的服务开发能力。

Flare 项目始于 2019 年，目前广泛应用于腾讯广告的各类后台服务，拥有数以万计的运行实例，在生产系统上经受了充分的考验。

2021 年 5 月，我们本着回馈社区、技术共享的精神正式将 Flare 开源。

## 特点

- 现代 C++ 设计风格，全面采用 C++20 的语法特性和标准库
- 提供 [M:N 线程模型](https://en.wikipedia.org/wiki/Thread_(computing)) 的用户态微线程实现 [Fiber](flare/doc/fiber.md)，让开发者用同步调用的写法获得异步调用的性能
- 支持基于消息的[流式 RPC](flare/doc/streaming-rpc.md)
- 除 RPC 外，还提供了一系列便利的[基础库](flare/base)：字符串、时间日期、编码、压缩、加解密、配置、HTTP 客户端等等，方便快速上手开发业务代码
- 提供了灵活的扩展机制，方便支持多种协议、服务发现、负载均衡、监控告警、[调用追踪](flare/doc/tracing.md)等
- 针对现代体系结构做了大量优化：[NUMA 感知](https://en.wikipedia.org/wiki/Non-uniform_memory_access)的[调度组](flare/doc/scheduling-group.md)、[对象池](flare/doc/object-pool.md)、[零拷贝缓冲区](flare/doc/buffer.md)等
- 高质量的代码。严格遵守 [Google C++ 代码规范](https://google.github.io/styleguide/cppguide.html)，测试覆盖率达 80%
- 完善的[文档](flare/doc)、[示例](flare/example)以及[调试支持](flare/doc/debugging.md)，方便快速上手

## 系统要求

### Linux

- Linux 3.10 或以上内核
- GCC 11 或以上版本的编译器（最早支持 `std::atomic<T>::wait` / `notify_*` 的 libstdc++ 版本，参见 [P1135](https://wg21.link/p1135)）
- x86-64 处理器；aarch64 和 ppc64le 也支持，但未在生产环境实际投入使用

### macOS

- macOS 13 (Ventura) 或以上
- Apple Clang（Xcode 14 及以上自带的 libc++ 已包含 `std::atomic<T>::wait` / `notify_*` 运行期符号）
- Apple Silicon (arm64) 与 Intel (x86_64) 均可

## 开始使用

Flare 是开箱即用的，已经自带了所需的[第三方库](thirdparty/)，通常不需要额外安装依赖。

`thirdparty/` 下的源码包通过 [Git LFS](https://git-lfs.github.com/) 存储，因此拉取代码前请确保 `git-lfs` 已正确安装。

### 构建

我们使用 [`blade`](https://github.com/blade-build/blade-build) 进行日常开发：

- 编译：`./blade build ...`
- 测试：`./blade test ...`

Flare 也支持 [`bazel`](https://bazel.build) 作为构建系统，参见 [bazel support](bazel/support/README.md)。

之后可以参考[入门导引](flare/doc/intro-rpc.md)，搭建一个简单的 RPC 服务。

### 调试

我们认为[调试体验](flare/doc/debugging.md)是开发维护过程中很重要的一环，为此提供了：

- [GDB 插件，用于列出挂起的 fiber](flare/doc/gdb-plugin.md)
- [对各种 Sanitizers 的支持](flare/doc/sanitizers.md)

### 测试

为改善编写单测的体验，我们提供了一些[测试工具](flare/doc/testing.md)，包括但不限于：

- [RPC Mock](flare/testing/rpc_mock.h)
- [Redis Mock](flare/testing/redis_mock.h)
- [COS Mock](flare/testing/cos_mock.h)
- [HTTP Mock](flare/testing/http_mock.h)
- [非虚函数 Mock](flare/testing/hooking_mock.h)
- 以及若干工具方法等

### 示例

我们提供了一些[使用示例](flare/example)，下面是一个简单的转发服务（同时演示了 RPC 客户端与服务端的使用）：

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

Flare 内部[基于 M:N 用户态线程](flare/doc/fiber.md)实现，因此通过 Flare 同步请求外部服务、调用 [Flare 内置的各种客户端](flare/net) 的同步接口都不会带来性能问题。如有更复杂的并发或异步需求，可参考 [Fiber 文档](flare/doc/fiber.md)。

另外，示例中的 `*.flare.pb.h` 通过 [Flare 的 Protocol Buffers 插件](flare/doc/pb-plugin.md) 生成。相对于 Protocol Buffers 原生的 `cc_generic_services`，这种方式生成的接口更易用。

## 更复杂的示例

实际使用中往往需要并发请求多种后端，下面的示例展示了如何在 Flare 中实现这种模式：

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

这个示例中，我们：

- 通过三种不同的客户端（[HTTP](flare/net/http/http_client.h)、[腾讯云 COS](flare/net/cos/cos_client.h)、RPC）发起了三个异步请求
- 通过 `flare::fiber::BlockingGet` 同步等待所有请求完成——这里只会阻塞用户态线程，不会有性能问题
- 输出日志展示各个服务的响应

出于展示目的，这里请求的是三个异构服务。如有需要，也可以用同样的方式请求同构服务、或同构异构混合的服务。

## 参与开发

我们非常欢迎共建。希望了解 Flare 更多内部设计的开发者、或需要对 Flare 进行二次开发的开发者，可参考 [`flare/doc/`](flare/doc/) 下的技术文档。

详情请参考 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 性能

由于业务需求的特点，我们在设计中更倾向于优化延迟及其平稳性，而非吞吐；但在这一前提下也尽力兼顾性能。

简单的对比数据可见[初步性能数据](flare/doc/benchmark.md)。

## 致谢

- 我们的底层实现大量参考了 [brpc](https://github.com/apache/incubator-brpc) 的设计
- RPC 部分，[grpc](https://grpc.io/) 给了我们很多启发
- 我们依赖了不少来自开源社区的[第三方库](thirdparty/)，正是站在巨人的肩膀上让我们得以更快更好地开发本项目，也因此我们也乐于回馈社区

在此向上述项目一并致以谢意。
