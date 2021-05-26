# HBase

我们提供了针对[HBase](https://hbase.apache.org)的服务端及客户端协议的支持。

尽管HBase底层同样通过Protocol Buffers进行序列化/反序列化，但是考虑到HBase在编程接口上与其他我们内置或外界常见的基于Protocol Buffers的RPC协议诸多不同（如“独特的”基于[`ExceptionResponse`](https://docs.cloudera.com/HDPDocuments/HDP2/HDP-2.4.0/bk_hbase_java_api/org/apache/hadoop/hbase/protobuf/generated/RPCProtos.ExceptionResponse.html)的错误处理等），我们将HBase作为独立的协议来实现。

除协议无关的框架之外，我们提供了如下一些HBase特有的类用于提供基于HBase的RPC协议的服务：

- `HbaseService`：当一个进程内提供了多个[`serviceName`](https://hbase.apache.org/2.0/devapidocs/org/apache/hadoop/hbase/ipc/ConnectionId.html#serviceName)不同的RPC服务时，我们通过这一类用于将各个RPC请求分发至不同的[`google::protobuf::Service`](https://developers.google.com/protocol-buffers/docs/reference/cpp/google.protobuf.service)（的子类）中。

在向框架注册自行实现的`XxxServiceImpl`时需要通过这个类完成。

- `HbaseServerController`：这个类提供了RPC服务端查询本次RPC相关信息，或控制RPC响应的能力，比如：
  - 通过`GetCellBlockCodec()`获取连接建立时协商的cell-block codec。
  - 通过`SetException(...)`返回错误给请求方。

以及如下一些类用户请求HBase服务：

- `HbaseChannel`：这个类代表了了一个连接某个服务的连接，通常被传递给`XxxService_Stub`来进行RPC。
- `HbaseClientController`：这个类用于控制RPC行为并获取服务端返回的错误信息（如果有的话）。

## C++接口

这一节介绍了我们提供了C++ API。我们建议阅读相应类的定义及文件注释获取更全面的信息。

### `HbaseService`

这个类提供了`AddService`用于添加用户实现的基于Protocol Buffers的服务。通常可以通过`Server::GetBuiltinNativeService<HbaseService>()`访问这个类的实例。如：

```cpp
class DummyService : public testing::EchoService {
  // ...
};

DummyService impl
Server server;

server.GetBuiltinNativeService<HbaseService>()->AddService(&impl);
```

### `HbaseServerController`

这个类提供了查询连接参数（`GetCellBlockCodec()`、`GetCellBlockCompressor()`、`GetEffectiveUser()`等）及返回错误（`SetException(...)`）等能力。

### `HbaseChannel`

这个类用于建立与服务器之间的连接，通过`HbaseChannel::Options`可以指定如cell-block codec等参数。

### `HbaseClientController`

这个类提供了一系列用户控制RPC的选项，并可以用于获取服务器返回的cell-block（如果有的话，`GetCellBlock()`）或错误信息（`GetException()`）等。

## 示例

这一节提供了一个简单的基于HBase协议的客户端/服务端的示例。

我们暂时尚未增加针对HBase协议的Flare的RPC插件，后续我们可能会通过插件生成更加简化（如直接提供`HbaseServiceController`，避免业务代码自行转换类型）的接口。

完整的示例代码可以参考[example/rpc/hbase](../../example/rpc/hbase)。

### 服务端

```cpp
class EchoServiceImpl : public EchoService {
 public:
  void Echo(google::protobuf::RpcController* controller,
            const EchoRequest* request, EchoResponse* response,
            google::protobuf::Closure* done) {
    auto ctlr = flare::down_cast<flare::HbaseServerController>(controller);
    if (!ctlr->GetRequestCellBlock().Empty()) {
      ctlr->SetResponseCellBlock(flare::CreateBufferSlow(
          "Echoing: " + flare::FlattenSlow(ctlr->GetRequestCellBlock())));
    }
    response->set_body(request->body());
    done->Run();
  }
};

int Entry(int argc, char** argv) {
  EchoServiceImpl service_impl;
  flare::Server server;

  server.AddProtocol("hbase");
  server.GetBuiltinNativeService<flare::HbaseService>()->AddService(
      &service_impl);
  server.ListenOn(flare::EndpointFromIpv4(FLAGS_ip, FLAGS_port));
  FLARE_LOG_FATAL_IF(!server.Start(), "Cannot start server.");

  flare::WaitForQuitSignal();
  server.Stop();
  server.Join();
  return 0;
}
```

### 客户端

```cpp
int Entry(int, char**) {
  flare::HbaseChannel::Options options = {
      .effective_user = "someone",
      .service_name = "example.hbase_echo.EchoService"};
  flare::HbaseChannel channel;
  FLARE_CHECK(channel.Open(FLAGS_server_addr, options));

  EchoService_Stub stub(&channel);
  EchoRequest req;
  EchoResponse resp;
  flare::HbaseClientController ctlr;
  req.set_body(FLAGS_echo_body);
  if (!FLAGS_cell_block.empty()) {
    ctlr.SetRequestCellBlock(flare::CreateBufferSlow(FLAGS_cell_block));
  }
  stub.Echo(&ctlr, &req, &resp, nullptr);

  FLARE_LOG_INFO("Succeeded: {}, Error Text: [{}].", !ctlr.Failed(),
                 ctlr.Failed() ? ctlr.ErrorText() : "");
  FLARE_LOG_INFO("Received: {}", resp.body());
  if (auto&& cb = ctlr.GetResponseCellBlock(); !cb.Empty()) {
    FLARE_LOG_INFO("Cell-block: {}", flare::FlattenSlow(cb));
  }
  return 0;
}
```
