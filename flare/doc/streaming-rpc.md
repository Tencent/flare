# 流式RPC

除普通的一问一答的RPC形式之外，flare还提供了[服务端流式RPC](https://grpc.io/docs/guides/concepts/#server-streaming-rpc)，[客户端流式RPC](https://grpc.io/docs/guides/concepts/#client-streaming-rpc)以及[双向流式RPC](https://grpc.io/docs/guides/concepts/#bidirectional-streaming-rpc)的支持。

取决于具体[协议](protocol.md)，并非所有协议都支持流式RPC。对于流式RPC的场景，通常我们推荐使用[flare内置的二进制协议](protocol/protocol-buffers.md)。

为标识一个方法为流式方法，可通过在[定义服务时将请求和/或响应使用`stream`关键字修饰](https://developers.google.com/protocol-buffers/docs/proto3#services)。（目前Protocol Buffers官方文档尚未文档化此关键字，但[`gRPC`的文档](https://grpc.io/docs/tutorials/basic/cpp/#defining-the-service)中对此作了介绍。）

对于老式代码，通过也可以通过`gdt.streaming_response`这一`option`来标记方法是服务端流式RPC。这一`option`的作用和使用`returns (stream XxxResponse)`相同：

*除非有兼容性需求，否则我们不再推荐使用这一自定义`option`。*

```protobuf
syntax = "proto3";

// `gdt.streaming_response` is defined in it.
import "common/rpc/rpc_options.proto";

// ... (Definition of `EchoRequest` & `EchoResponse`.)

service EchoService {
  rpc EchoStreamResponse(EchoRequest) returns (stream EchoResponse);
  rpc EchoStreamResponse2(EchoRequest) returns (stream EchoResponse) {
    option (gdt.streaming_response) = true;
  }
}
```

其中`EchoStreamResponse`和`EchoStreamResponse2`实际接口（服务端及客户端）相同。

对于新接口，我们推荐使用官方的`stream`关键字来标识。

为方便进行流式RPC的消息读写，我们提供了[`StreamReader<T>`](../rpc/internal/stream.h)、[`StreamWriter<T>`](../rpc/internal/stream.h)。具体使用方式参见文件中注释。

---
[返回目录](README.md)
