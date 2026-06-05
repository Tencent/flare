# 增加新协议

flare的多协议支持参见[协议](../protocol.md)。本文从扩展者的视角列出实现一个新协议所需要的步骤。建议先阅读[协议](../protocol.md)了解整体设计，再以[`Http11Protocol`](../../rpc/protocol/http/http11_protocol.h)、[`StdProtocol`](../../rpc/protocol/protobuf/std_protocol.h)等已有实现作为参考。

## 总体步骤

1. 实现一个[`StreamProtocol`](../../rpc/protocol/stream_protocol.h)的子类，负责从字节流中切分、解析消息以及把消息序列化回字节流。
2. 实现一个[`Message`](../../rpc/protocol/message.h)的子类，作为框架内部传递的消息对象。
3. 实现一个[`Controller`](../../rpc/protocol/controller.h)的子类，承载该协议的请求上下文。
4. 实现一个[`StreamService`](../../rpc/protocol/stream_service.h)的子类（仅服务端需要），负责将协议消息分发给业务实现。
5. 通过`FLARE_RPC_REGISTER_*_STREAM_PROTOCOL*`系列宏将`StreamProtocol`注册进框架。注册之后，业务侧调用`Server::AddProtocol("xxx")`或在`RpcChannel`的URI中使用`xxx://`即可启用。

## `StreamProtocol`接口要点

```cpp
class StreamProtocol {
 public:
  virtual const Characteristics& GetCharacteristics() const = 0;
  virtual const MessageFactory* GetMessageFactory() const = 0;
  virtual const ControllerFactory* GetControllerFactory() const = 0;

  virtual MessageCutStatus TryCutMessage(
      NoncontiguousBuffer* buffer,
      std::unique_ptr<Message>* message) = 0;

  virtual bool TryParse(std::unique_ptr<Message>* message,
                        Controller* controller) = 0;

  virtual void WriteMessage(const Message& message,
                            NoncontiguousBuffer* buffer,
                            Controller* controller) = 0;
};
```

- `TryCutMessage`在[IO线程](../io.md)中调用，应尽快返回。它返回`MessageCutStatus`枚举：`NotIdentified`/`NeedMore`/`Cut`/`ProtocolMismatch`/`Error`。服务端识别协议时会依次尝试所有已注册的`StreamProtocol`，因此当字节流不属于本协议时务必返回`ProtocolMismatch`并**保持`buffer`不变**。
- `TryParse`在worker fiber中调用，承担昂贵的解析工作。`TryCutMessage`可只做最小程度的切分，把重活留给`TryParse`以提高并行度。
- `WriteMessage`将消息序列化到`buffer`，由框架的IO层写出。
- `Characteristics::name`仅用于展示；真正的协议名通过注册宏指定。

## 注册到框架

```cpp
// 推荐：通过工厂构造（构造函数可携带参数）
FLARE_RPC_REGISTER_SERVER_SIDE_STREAM_PROTOCOL_ARG(
    "myproto", MyStreamProtocol, /* ctor args... */);
FLARE_RPC_REGISTER_CLIENT_SIDE_STREAM_PROTOCOL_ARG(
    "myproto", MyStreamProtocol, /* ctor args... */);

// 或：通过类名直接注册（无构造参数）
FLARE_RPC_REGISTER_SERVER_SIDE_STREAM_PROTOCOL("myproto", MyStreamProtocol);
FLARE_RPC_REGISTER_CLIENT_SIDE_STREAM_PROTOCOL("myproto", MyStreamProtocol);
```

服务端、客户端的注册表是独立的，分别通过`server_side_stream_protocol_registry`、`client_side_stream_protocol_registry`管理；如果某个协议只需要在一端出现（例如只做客户端），仅注册对应一侧即可。

## `StreamService`要点

服务端还需要实现[`StreamService`](../../rpc/protocol/stream_service.h)将解析出的消息分发到业务逻辑。关键虚函数：

- `Inspect`：快速判断该`Message`是否属于自己，识别失败则框架会继续尝试下一个`StreamService`。
- `FastCall` / `StreamCall`：分别处理普通RPC和[流式RPC](../streaming-rpc.md)。
- `GetUuid`：返回本Service的唯一标识。

业务通过`Server::AddService(...)`注册`StreamService`实例（或通过Protocol Buffers生成的`Xxx_Stub`间接注册）。

## 客户端

客户端的协议入口是用户构造的`XxxChannel`（或基于`RpcChannel`的Stub）。新协议如果沿用现有的`RpcChannel`，主要工作是实现上述的`StreamProtocol`；如果需要不同的用户接口，可参考[`HbaseChannel`](../../net/hbase/hbase_channel.h)、[`RedisClient`](../../net/redis/redis_client.h)等独立Channel类的做法。

## 参考实现

| 协议 | 入口 | 备注 |
| --- | --- | --- |
| Protocol Buffers（flare原生） | [`StdProtocol`](../../rpc/protocol/protobuf/std_protocol.h) | 最完整的示例，含压缩、流式、attachment等特性 |
| HTTP/1.x | [`Http11Protocol`](../../rpc/protocol/http/http11_protocol.h) | 协议识别和HTTP特性处理的典型实现 |
| HBase | [`HbaseClientProtocol`](../../net/hbase/hbase_client_protocol.h) / [`HbaseServerProtocol`](../../net/hbase/hbase_server_protocol.h) | 客户端使用独立的`HbaseChannel` |
| Redis | [`RedisProtocol`](../../net/redis/redis_protocol.h) | 同上，独立的`RedisClient` |

---
[返回目录](../README.md)
