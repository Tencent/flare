# Flare 后台服务开发框架

Flare Backend Service Framework

[English Document](README-en.md)

[![license NewBSD](https://img.shields.io/badge/license-BSD-yellow.svg)](LICENSE)
[![C++](https://img.shields.io/badge/language-C++-blue.svg)](https://isocpp.org/)
[![Code Style](https://img.shields.io/badge/code%20style-google-blue.svg)](https://google.github.io/styleguide/cppguide.html)
[![Platform](https://img.shields.io/badge/platform-linux%20-lightgrey.svg)](https://www.kernel.org/)

[腾讯广告](https://e.qq.com/ads/) 是腾讯公司最重要的业务之一，其后台大量采用 C++ 开发。

Flare 是我们吸收先前服务框架和业界开源项目及最新研究成果开发的现代化的后台服务开发框架，旨在提供针对目前主流软硬件环境下的易用、高性能、平稳的服务开发能力。

Flare 项目开始于 2019 年，目前广泛应用于腾讯广告的众多后台服务，拥有数以万计的运行实例，在实际生产系统上经受了足够的考验。

2021 年 5 月，本着回馈社区、技术共享的精神，正式对外开源。

## 特点

- 现代 C++ 设计风格，广泛采用了 C++11/14/17/2a 的新的语法特性和标准库
- 提供了 [M:N 的线程模型](https://en.wikipedia.org/wiki/Thread_(computing))的微线程实现[Fiber](flare/doc/fiber.md)，方便业务开发人员以便利的同步调用语法编写高性能的异步调用代码
- 支持基于消息的[流式 RPC](flare/doc/streaming-rpc.md)支持
- 除了 RPC 外，还提供了一系列便利的[基础库](flare/base)，比如字符串、时间日期、编码处理、压缩、加密解密、配置、HTTP 客户端等，方便快速上手开发业务代码
- 提供了灵活的扩充机制。方便支持多种协议、服务发现、负载均衡、监控告警、[调用追踪](flare/doc/tracing.md)等
- 针对现代体系结构做了大量的优化。比如 [NUMA 感知](https://en.wikipedia.org/wiki/Non-uniform_memory_access)的[调度组](flare/doc/scheduling-group.md)和[对象池](flare/doc/object-pool.md)、[零拷贝缓冲区](flare/doc/buffer.md)等
- 高质量的代码。严格遵守 [Google C++ 代码规范](https://google.github.io/styleguide/cppguide.html)，测试覆盖率达 80%
- 完善的[文档](flare/doc)和[示例](flare/example)以及[调试支持](flare/doc/debugging.md)，方便快速上手

## 系统要求

- Linux 3.10 或以上内核，暂不支持其他操作系统
- x86-64 处理器，也支持 aarch64 及 ppc64le，但是未在生产环境上实际使用过
- GCC 8 或以上版本的编译器

## 开始使用

Flare 是开箱即用的，已经自带了所需的[第三方库](thirdparty/)，因此通常不需要额外安装依赖库。只需要在 Linux 下，拉取代码，即可使用。

`thirdparty/`下面的压缩包我们通过[Git LFS](https://git-lfs.github.com/)存储，因此在拉取代码之前您需要确保`git-lfs`已经正确的安装了。

### 构建

我们使用[`blade`](https://github.com/chen3feng/blade-build)进行日常开发。

- 编译：`./blade build ...`，
- 测试：`./blade test ...`。

之后就可以参考[入门导引](flare/doc/intro-rpc.md)中的介绍，搭建一个简单的RPC服务了。

### 调试

我们相信，[调试](flare/doc/debugging.md)体验也是开发维护过程中很重要的一部分，我们为此也做了如下一些支持：

- [GDB插件用于列出挂起的fibers](flare/doc/gdb-plugin.md)
- [支持各种Sanitizers](flare/doc/sanitizers.md)

### 测试

为了改善编写单测的体验，我们提供了一些用于[编写单测的工具](flare/doc/testing.md)。

这包括但不限于：

- [RPC Mock](flare/testing/rpc_mock.h)
- [Redis Mock](flare/testing/redis_mock.h)
- [COS Mock](flare/testing/cos_mock.h)
- [HTTP Mock](flare/testing/http_mock.h)
- [非虚函数Mock](flare/testing/hooking_mock.h)
- 部分工具方法等

### 示例

我们提供了一些[使用示例](flare/example)以供参考，下面是一个简单的转发服务（同时包含RPC客户端及服务端的使用）。

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

Flare内部[基于M:N的用户态线程](flare/doc/fiber.md)实现，因此通过Flare同步的请求外界服务、使用[Flare内置的各种客户端](flare/net)的同步接口均不会导致性能问题。如果有更复杂的并发或异步等需求可以参考[我们的文档](flare/doc/fiber.md)。

另外，示例中`*.flare.pb.h`通过[我们的Protocol Buffers插件](flare/doc/pb-plugin.md)生成。这样生成的接口相对于Protocol Buffers生成的`cc_generic_services`而言，更易使用。

## 更复杂的示例

实际使用中，往往会面对需要并发请求多种后端的场景，下面的示例介绍了如何在Flare中进行这种操作：

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

- 通过三种不同的客户端（[HTTP](flare/net/http/http_client.h)、[腾讯云COS](flare/net/cos/cos_client.h)、RPC）发起了三个异步请求；
- 通过`flare::fiber::BlockingGet`同步等待所有请求完成。这儿我们只会阻塞用户态线程，不会存在性能问题；
- 打印日志输出各个服务的响应。

出于展示目的，我们这儿请求了三个异构的服务。如果有必要，也可以通过这种方式请求同构的、或者部分同构部分异构的服务。

## 参与开发

我们非常欢迎参与共同建设，对于希望了解 Flare 更多内部设计的开发者，或需要对 Flare 进行二次开发的开发者而言，[`flare/doc/`](flare/doc/)下有更多的技术文档可供参考。

详情请参考[CONTRIBUTING.md](CONTRIBUTING.md)。

## 性能

由于业务需求的特点，我们在设计过程中更倾向于优化延迟及抖动的平稳性而非吞吐，但是也在这个前提下尽量保证性能。

出于简单的对比目的，我们提供了[初步的性能数据](flare/doc/benchmark.md)。

## 致谢

- 我们的底层实现大量参考了[brpc](https://github.com/apache/incubator-brpc)的设计。
- RPC部分，[grpc](https://grpc.io/)给了我们很多启发。
- 我们依赖了不少开源社区的[第三方库](thirdparty/)，站在巨人的肩膀上使得我们可以更快更好地开发本项目，也因此积极地回馈给开源社区。

在此，我们对上述项目一并致以谢意。
