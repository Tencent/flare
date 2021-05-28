# Flare 后台服务开发框架

Flare 是一个现代化的后台服务开发框架，旨在提供针对目前主流软硬件环境下的易用、高性能、平稳的服务开发能力。

目前 Flare 广泛投产于腾讯广告，并拥有数以万计的运行实例。

## 开始使用

Flare 已经自带了所需的[第三方库](thirdparty/)，因此通常不需要额外安装依赖库。

为了编译 Flare，需要GCC 8或更新版本的支持。

### 开发

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
#include "thirdparty/gflags/gflags.h"

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

## 二次开发

另外，对于希望了解 Flare 更多内部设计的开发者，或需要对 Flare 进行二次开发的开发者而言，[`flare/doc/`](flare/doc/)下有更多的技术文档可供参考。

## 性能

虽然我们在设计过程中更倾向于优化延迟而非吞吐，但是出于简单的对比目的，我们提供了[初步的性能数据](flare/doc/benchmark.md)。

## 致谢

- 我们的底层实现大量参考了[brpc](https://github.com/apache/incubator-brpc)的设计；
- RPC部分[grpc](https://grpc.io/)给了我们很多启发。

在此，我们对上述项目一并致以谢意。
