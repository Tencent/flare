# Protocol Buffers插件

为了生成我们期望的 RPC 接口，我们开发了相应的 Protobuf 插件来生成相关的代码。

[Protocol Buffers](https://developers.google.com/protocol-buffers)默认[通过`cc_generate_services = true;`生成的](https://developers.google.com/protocol-buffers/docs/reference/cpp-generated#service)接口较为简陋，且没有对流式RPC直接提供支持。因此我们提供了[自己的pb插件](../rpc/protocol/protobuf/plugin/)用于生成这些类。

## 使用

我们参考trpc的`trpc_proto_library`、[grpc的`cc_grpc_library`](https://github.com/grpc/grpc/blob/master/bazel/cc_grpc_library.bzl)的实现，提供了必要的`protoc`插件来生成适配于Flare的接口（`*.flare.pb.{h,cc}`），下面列出了必要的修改。

**与trpc/grpc默认会同时生成`*.pb.{h,cc}`、`*.(trpc|grpc).{h,cc}`不同，`cc_flare_library`只会生成`*.flare.{h,cc}`**。按照grpc的注释，后续`cc_grpc_library.grpc_only`会默认为`True`，即默认只生成`*.grpc.{h,cc}`。因此，长期而言，flare将和grpc保持一致。

`BUILD`文件引用`//flare/tools/build_rules.bld`：

```python
load('//flare/tools/build_rules.bld', 'cc_flare_library')
```

需要注意的是，`cc_flare_library`只会生成`*.flare.pb.{h,cc}`，因此需要先通过`proto_library`定义相关的`*.pb.{h,cc}`的目标：

*我们通常建议`cc_flare_library`的目标的`name`以`_flare`结尾。*

```python
# Protocol Buffers message target.
proto_library(
  name = 'echo_service_proto',
  srcs = 'echo_service.proto'
)

# Flare-friendly target.
cc_flare_library(
  name = 'echo_service_proto_flare',
  srcs = 'echo_service.proto',
  deps = [
    ':echo_service_proto',
  ],
)
```

### 示例

[如下代码](../example/rpc/BUILD)给出了一个使用`relay_service.flare.pb.h`的示例：

```python
load('//flare/tools/build_rules.bld', 'cc_flare_library')

# Generates `relay_service.pb.{h,cc}`.
proto_library(
  name = 'relay_service_proto',
  srcs = 'relay_service.proto',
  deps = [
    '//flare/rpc:rpc_options_proto',
  ],
)

# Generates `relay_service.flare.pb.{h,cc}`.
cc_flare_library(
  name = 'relay_service_flare_proto',
  srcs = 'relay_service.proto',
  deps = [
    ':relay_service_proto',
  ]
)

# Uses `relay_service.flare.pb.h`.
cc_binary(
  name = 'relay_server',
  srcs = 'relay_server.cc',
  deps = [
    ':relay_service_proto_flare',
    # ...
  ]
)
```

## Flare版本接口

以如下proto为例：

```protobuf
syntax = "proto3";

package echo;

message EchoRequest {
  string body = 1;
}

message EchoResponse {
  string body = 2;
}

service EchoService {
  rpc Echo(EchoRequest) returns(EchoResponse);
  rpc EchoStreamRequest(stream EchoRequest) returns (EchoResponse);
  rpc EchoStreamResponse(EchoRequest) returns (stream EchoResponse);
  rpc EchoStreamBoth(stream EchoRequest) returns (stream EchoResponse);
}
```

*Flare插件同样支持已有的`gdt.streaming_response`选项，其作用于`returns (stream Xxx)`相同。对于新代码，我们推荐使用Protocol Buffers内置的`stream`关键字。*

### 服务端

Flare插件会生成如下接口（有所简化，省略掉部分非对外的细节）：

```cpp
class SyncEchoService {
 public:
  virtual void Echo(const EchoRequest& request, EchoResponse* response,
                    flare::RpcServerController* controller);

  virtual void EchoStreamRequest(flare::StreamReader<EchoRequest> reader,
                                 EchoResponse* response,
                                 flare::RpcServerController* controller);

  virtual void EchoStreamResponse(const EchoRequest& request,
                                  flare::StreamWriter<EchoResponse> writer,
                                  flare::RpcServerController* controller);

  virtual void EchoStreamBoth(flare::StreamReader<EchoRequest> reader,
                              flare::StreamWriter<EchoResponse> writer,
                              flare::RpcServerController* controller);
};
```

各个接口输入输出含义自明，不再赘述。

### 客户端

对于客户端，Flare的插件会分别生成同步的接口及异步的接口（基于[`Future<...>`](../base/future.h)）。

如果需要并发请求多个后端，无论是否需要同步等待所有后端返回，请参考“异步接口”一节。

#### 扩展

我们生成的`Stub`类增加了一个可以直接接受`uri`（对应于`RpcChannel::Open`的相应参数）的重载版本。这允许用户将“打开channel+构造stub”合为一步，简化开发：

```cpp
EchoService_SyncStub stub("flare://some.polaris.address");

// Use `stub` here.
```

如果传入的地址打开失败，那么在这个`stub`上后续进行的RPC均会以[`rpc::STATUS_INVALID_CHANNEL`](../rpc/protocol/protobuf/rpc_meta.proto)失败。

#### 同步接口

当需要请求单个后端或**串行**请求多个后端时，可以使用同步接口。

Flare生成的接口如下（有所简化，省略了部分非对外的细节）：

```cpp
class EchoService_SyncStub {
 public:
  flare::Expected<EchoResponse, flare::Status> Echo(
      const EchoRequest& request, flare::RpcClientController* controller);

  std::pair<flare::StreamReader<EchoResponse>,
            flare::StreamWriter<EchoRequest>>
  EchoStreamRequest(flare::RpcClientController* controller);

  flare::StreamReader<EchoResponse> EchoStreamResponse(
      const EchoRequest& request, flare::RpcClientController* controller);

  std::pair<flare::StreamReader<EchoResponse>,
            flare::StreamWriter<EchoRequest>>
  EchoStreamBoth(flare::RpcClientController* controller);
};
```

`flare::Expected`与[P0323R7 std::expected](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0323r7.html)基本相同，出错时可直接通过`flare::Expected<T, flare::Status>`得到[错误码](pb-rpc-status.md)。

#### 异步接口

异步接口并不意味着只能用于异步场景。

我们的[fiber运行时可以和`Future<...>`配合使用](future.md)，因此，实际上我们预期更多使用异步接口的场景通常是用于并发请求多个后端（并通过[`flare::fiber::BlockingGet(...)`](../fiber/future.h) + [`flare::WhenAll`](../base/future.h)等待多个RPC完成）。

尽管可以通过[`flare::fiber::Async`](../fiber/async.h)配合同步接口来*模拟*异步调用，由于[fiber本身是一种受`vm.max_map_count`系统参数限制的资源](fiber.md)，**我们不建议这么做**。另外，Flare插件生成的异步接口的性能应当也优于通过模拟的实现。

Flare生成的接口如下（有所简化，省略了部分非对外的细节）：

```cpp
class EchoService_AsyncStub {
 public:
  flare::Future<flare::Expected<EchoResponse, flare::Status>> Echo(
      const EchoRequest& request, flare::RpcClientController* controller);

  std::pair<flare::AsyncStreamReader<EchoResponse>,
            flare::AsyncStreamWriter<EchoRequest>>
  EchoStreamRequest(flare::RpcClientController* controller);

  flare::AsyncStreamReader<EchoResponse> EchoStreamResponse(
      const EchoRequest& request, flare::RpcClientController* controller);

  std::pair<flare::AsyncStreamReader<EchoResponse>,
            flare::AsyncStreamWriter<EchoRequest>>
  EchoStreamBoth(flare::RpcClientController* controller);
};
```

可以注意到，除了通过`Future<T>`代替`T`之外，异步接口和同步基本相同。

另外，关于如何使用异步接口来*同步并发调用多个后端*，此处给出一个示例：

```cpp
// Initialize stubs and open channels to different backends.
EchoService_AsyncStub stub1("flare://some.polaris.address-1");
EchoService_AsyncStub stub2("flare://some.polaris.address-2");

// Prepare requests.
EchoRequest req1, req2;
req1.set_body("body1");
req2.set_body("body2");

// Issue RPCs asynchronously.
flare::RpcClientController ctlr1, ctlr2;
auto f1 = stub1.Echo(req1, &ctlr1);
auto f2 = stub2.Echo(req2, &ctlr2);

// Wait for all RPCs.
auto&& [res1, res2] = flare::fiber::BlockingGet(flare::WhenAll(&f1, &f2));

// Now both `res1` and `res2` have finished (either successfully or with an
// error), and are ready for inspection.
```

## 与`cc_generate_services`的兼容性

我们的插件生成类时会避免和`cc_generate_services`生成的类重名。此外，我们的插件检测到未指定`cc_generate_services = true;`时，会额外的`*.flare.pb.h`中生成与`cc_generate_services`接口相同的类以供使用。

## 与`gdt_future_rpc`的兼容性

Flare的pb插件同样会生成和`gdt_future_rpc`产出相同的接口。

---
[返回目录](README.md)
