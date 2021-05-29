# RPC 入门导引

互联网服务的后台普遍都是各种分布式系统，服务之间像函数之间进行调用。在网络编程的早期，程序员需要自己控制消息的编码和解码，以及收到消息后针对不同的命令字（也可能是个整数，不同的命令字代表不同的含义）的处理，十分繁琐容易出错，所以后来出现了多种技术来简化网络编程的复杂性。

[远程过程调用](https://zh.wikipedia.org/zh-cn/%E9%81%A0%E7%A8%8B%E9%81%8E%E7%A8%8B%E8%AA%BF%E7%94%A8)（Remote Procedure Call，简称 RPC） 就是其中最广泛的一种技术，通过封装底层的网络细节，让程序员可以像调用本地的函数一样去调远程函数，以简化工作的复杂性，并提高开发效率和质量。

RPC 的基本原理是，使用接口描述语言（IDL）描述协议，然后通过协议编译器生成客户端和服务器端的辅助代码。服务器端的开发人员实现好要被调用的函数，并注册到 RPC 服务框架中。先启动服务器端程序，监听服务器端的连接准备处理客户端的请求。调用过程如下：

- 客户端要发起调用时，调用协议编译器生成的客户端代理，把请求消息序列化（所谓打包），其中不但包含了消息本身，还包含了要调用的函数的信息（比如函数名或者函数ID等）并通过网络发送到服务器端。
- 服务器端收到客户端发来的请求后，解析出其中包含的函数标识以及请求消息（反序列化），通过协议编译器生成的辅助代码调用服务器开发者实现的服务处理函数。
- 然后框架再把回应的消息再次序列化，通过网络发送回客户端。
- 客户端收到回应后，把消息反序列化，最终得到调用结果。

以上只是 RPC 最基本的原理，完善的 RPC 实现还包括名字服务、负载均衡、过载保护、监控告警、诊断机制，以及各种性能优化等等。

下面简单介绍如何使用flare框架开发一个简单的 "Echo" 服务（收到什么消息都原样返回）。

## 程序环境初始化

**使用flare开发时，无论是开发服务端程序或是客户端程序，均需要对flare环境进行初始化。**

*flare环境的初始化包括但不限于flare的各依赖项及flare自身，如glog、gflags、fiber运行时等等。*

我们提供了`flare::Start`进行这项工作。这个函数接受一个用户回调（函数指针/functor/lambda表达式），并在调用回调前后对flare环境进行初始化及清理。

通常而言，对flare的初始化在`main`中调用：

```cpp
int UserMain(int argc, char** argv) {  // Method name here is not significant.
  // ...
}

int main(int argc, char** argv) {
  return flare::Start(argc, argv, UserMain);
}
```

之后程序自身的逻辑从`UserMain`开始。`UserMain`的返回值最终会由`flare::Start`作为`main`的返回值返回给系统。

### 捕获`envp`

`flare::Start`的第三个参数可以是lambda，因此如果在`UserMain`需要获取`envp`（即`main`的第三个参数），则可以自行在`main`中捕获：

```cpp
int main(int argc, char** argv, char** envp) {
  return flare::Start(argc, argv, [envp] { /* ... */ });
}
```

## Echo服务开发

我们从一个简单的 Echo 服务开始，来一步步说明如何开发一个 RPC 服务。

### 定义协议IDL

flare通常使用[Protocol Buffers](https://github.com/protocolbuffers/protobuf)作为其IDL。

我们假定echo服务的IDL定义如下（`echo_service.proto`）：

```protobuf
syntax = "proto3";

package example.protobuf_echo;

message EchoRequest {
  string body = 1;
}

message EchoResponse {
  string body = 2;
}

service EchoService {
  rpc Echo(EchoRequest) returns(EchoResponse);
}
```

为了能更好的贴合flare自有的一些特性及便利之处，我们建议使用[flare的pb插件](pb-plugin.md)来编译IDL文件（`*.proto`）。

flare的插件可以通过在`BUILD`文件中引用`//flare/tools/proto_library.bld`，并使用`flare_proto_library`来定义协议目标：

```python
flare_proto_library(
  name = 'echo_service_proto',
  srcs = 'echo_service.proto',
)
```

这样会生成`echo_service.flare.pb.{h,cc}`，使用时需要包含`echo_service.flare.pb.h`（而不是`echo_service.pb.h`）。

在使用flare的plugin生成之后，会生成`SyncEchoService`、`EchoService_SyncStub`两个类，分别用于服务端、客户端。

如果需要在现有的`proto_library`上使用flare的pb插件，可以参考[文档中](pb-plugin.md)的非侵入式使用方式。

### 实现服务器端

首先需要实现的是RPC逻辑本身：

```cpp
class EchoServiceImpl : public SyncEchoService {
 public:
  void Echo(const EchoRequest& request, EchoResponse* response,
            flare::RpcServerController* controller) override {
    response->set_body(request.body());
  }
};
```

我们需要定义一个服务类，名字无所谓但是通常是 proto 中定义的服务名加上 `Impl` 后缀，比如这里叫 `EchoServiceImpl`，用于实现插件生成的 `SyncEchoService` 接口。
注意这里的接口名比起 proto 文件中定义的的多了一个 `Sync` 前缀，这个前缀是插件自动添加的。

然后覆盖接口类中定义的所有虚函数，这里只有一个也就是 `Echo`。

注意函数定义结尾的 [`override` 关键字](https://zh.cppreference.com/w/cpp/language/override)，能帮助你在编译阶段就提前发现由于函数名拼写或者参数类型错误等导致的未能正确覆盖虚函数的错误。

request 和 response 对象都已经准备好了，你只需要从 request 中读取请求里的信息，并根据里的需要正确地填充 response 对象。

然后剩下的事情就都由框架替你完成了。

`controller` 对象是一个可读写对象，你可以从中读取到各种附加信息，比如客户端的 IP 地址等等，也可以用于把本次请求设置为失败，在介绍阶段我们可以不深入理解。

### 服务端入口

编写服务端初始化代码并启动服务：

```cpp
namespace example::protobuf_echo {

int Entry(int argc, char** argv) {
  // Although not strictly required here, it's suggested to provide a service
  // name so that more advanced features (e.g., distributed tracing) will be
  // available when needed.
  flare::Server rpc_server{flare::Server::Options{
    .service_name = "echo"  // See descriptions below.
  }};

  // @sa: Section [支持多种协议]
  rpc_server.AddProtocol("flare");

  // Add your service implementation.
  rpc_server.AddService(std::make_unique<EchoServiceImpl>());

  // Bind to specified address, listen for client connections, and start.
  rpc_server.ListenOn(flare::EndpointFromIpv4(FLAGS_ip, FLAGS_port));
  FLARE_CHECK(rpc_server.Start());

  // Wait forever until being interrupted (e.g. Ctrl-C)
  flare::WaitForQuitSignal();
  rpc_server.Stop();
  rpc_server.Join();

  return 0;
}

}  // namespace example::*protobuf_echo*

int main(int argc, char** argv) {
  return flare::Start(argc, argv, example::protobuf_echo::Entry);
}
```

*尽管严格来说对于这儿不需要，但是我们建议启动[`flare::Server`](../rpc/server.h)时通过`flare::Server::Options::service_name`指定当前服务的服务名。某些高级功能（如[RPC追踪](tracing.md)、[RPC录制回放](rpc-log-and-dry-run.md)等）将会需要这一信息。*

至此，一个完整的echo服务就完成了。

#### 支持多种协议

示例中只注册了一个 `flare` 协议，flare 支持服务在同一端口上同时注册多种不同的协议。

支持更多的协议可以通过`flare::Server::AddProtocols`完成：

```cpp
// ...
rpc_server.AddProtocols({"flare", "http+gdt-json", "http+pb", "qzone-pb"});
```

关于协议开发及使用，参考[协议](protocol.md)。

#### 实现更复杂的业务逻辑

flare使用单独的[fiber](fiber.md)调用接口的实现代码，因此无需（并且不建议）创建线程池来执行业务代码。

由于flare已经通过用户态调度fiber实现了轻量的上下文切换，因此也无需（并且不建议）尝试使用异步设计来开发业务代码。

## 创建客户端

客户端也需要先初始化flare运行时，然后就可以通过使用flare的pb插件生成的`*_SyncStub`来发起 RPC 调用。

发起RPC之前首先需要打开一个连接目标机器/集群的`RpcChannel`（可以理解为一个逻辑上的“连接”）：

```cpp
flare::RpcChannel channel;
if (!channel.Open("http+pb://127.0.0.1:12345")) {
  // Raise error.
}
```

关于URI的命名规范及`RpcChannel`的使用，参见[channel.md](channel.md)。

还需要创建一个 `RpcClientController` 对象，用于对于单次RPC的行为的控制（如超时等）以及获取调用的额外信息（比如耗时，错误信息等）：

```cpp
flare::RpcClientController controller;
```

还需要使用插件生成的客户端代理类（类名为服务名加上 `_SyncStub` 后缀），本示例中为 `EchoService_SyncStub` 来发起 RPC：

```cpp
EchoService_SyncStub stub(&channel);

EchoRequest request;
request.set_body("hello");
auto resp = stub.Echo(request, &controller);
```

除[流式RPC](streaming-rpc.md)外，flare的pb插件生成的接口均是如下形式：

```cpp
flare::Expected<Response, flare::Status> MethodName(
    const Request& request, flare::RpcClientController* controller);
```

其返回值类型 `flare::Expected` 是一个表示“要么有值要么有错误”的高级联合类型，使用方式同[`std::experimental::expected`](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0323r7.html)。

可以通过`!` 运算符来检测调用是否成功。当调用失败时，通过 `.error()` 获得错误信息；当调用成功时，通过 `->` 或者 `*` 运算符获得回应的内容。还可以通过`RpcClientController`里的方法来访问更详细的调用结果信息。

至此，可以通过`stub.Echo`来请求服务端并获取返回值了：

```cpp
flare::Expected<EchoResponse, flare::Status> rc = stub.Echo(
    request, &controller);
if (!rc) {
  FLARE_LOG_WARNING("Failed: {}", rc.error().ToString());
} else {
  FLARE_LOG_INFO("Received: {}", rc->body());
}
```

另外，为了简化开发，我们允许将打开`RpcChannel`和创建`EchoService_SyncStub`合并为一步：

```cpp
// All subsequent RPCs would fail if the channel cannot be opened.
EchoService_SyncStub stub("flare://some.polaris.address");
```

请注意，RPC的发起**必须**在flare的运行时环境之中。即，**RPC不能在由业务创建的线程或线程池中发起**。这一行为类似于许多UI框架不允许在业务自行创建的线程中操作UI控件。在必要的时候，可以考虑[通过`Future`完成业务自行维护的线程和flare运行时之间的互相通信。](fiber.md)

---
在 [example/](../example/)下面可以找到更多的示例代码。

---
[返回目录](README.md)
