# 协议

为兼容公司内复杂的RPC环境，flare尝试尽可能支持多种协议。

协议这一概念体现在两种层面：

- 最终用户面对的C++类型：
  - [Protocol Buffers](protocol/protocol-buffers.md)
  - [HTTP](protocol/http.md)（这儿特指真正的HTTP请求，而非通过HTTP承载的pb请求）
  - ...
- 用于编解码请求的[线上格式（Wire protocol）](https://en.wikipedia.org/wiki/Wire_protocol)。多种不同的线上格式可能最终面对用户的C++接口是一样的。以Protocol Buffers为例，其线上格式可能为：
  - [flare自有协议](protocol/protocol-buffers.md)
  - 广告线现有的承载于HTTP之上的pb协议
  - ...

读者在阅读协议相关的文档时可能需要根据上下文自行区分。

对于最终用户而言，协议通过字符串标识，如（以Protocol Buffers为例）打开[`RpcChannel`](../rpc/rpc_channel.h)时使用的[URI](https://en.wikipedia.org/wiki/Uniform_Resource_Identifier)中的`scheme`段，以及初始化[`Server`](../rpc/server.h)时调用`Server::AddProtocol`时传入的协议名。

## 内置协义

关于内置协义相关信息，可以参考下列文档：

- [增加新协议](protocol/new.md)
- [HTTP](protocol/http.md)
- [Protocol Buffers](protocol/protocol-buffers.md)
- [Redis](protocol/redis.md)
- [HBase](protocol/hbase.md)
- [CKV](protocol/ckv.md)
- [JCE](protocol/jce.md)
- [TDBank](protocol/tdbank.md)

## 多协议支持的实现

我们通过接口（[`StreamProtocol`](../rpc/protocol/stream_protocol.h)的子类）来实现从二进制字节流直接翻译为上层RPC协议消息对象。针对不同的线上格式（on-wire format），我们实现了不同的`StreamProtocol`对象。由于框架始终和`StreamProtocol`交互，因此并不感知各个协议之间的不同。

各个协议可能生成**C++类型不同的消息**（[`Message`](../rpc/protocol/message.h)的子类）类型，并由对协议有所感知的其他类进行处理（见后文）。

由于其天然的区别，服务端于客户端对多协议支持采取不同的方法。

### 服务端

我们的老框架已经提供了HTTP、QZone协议的自动识别，Flare中我们进一步参考[brpc](https://github.com/apache/incubator-brpc)的设计实现了通用的协议识别。@sa: [brpc/docs/cn/new_protocol.md](https://github.com/apache/incubator-brpc/blob/master/docs/cn/new_protocol.md)。

服务端在收到请求时是无法预先获知请求方所使用的协议的，因此需要`StreamProtocol::TryCutMessage`自行尝试识别请求方的协议，如果无法识别，则由框架尝试使用其他协议识别。

显然这无法保证100%的准确率。多个协议之间可能有类似乃至相同的特征，因此我们只提供“最大努力”的识别。在特定情况下，可能需要业务避免“一股脑”的将所有协议通过`Server::AddProtocol`加入服务端。

*目前我们内置的协议均有其较明显的特征，尚不存在出现无法识别的情况。*

需要注意的是，`TryCutMessage`只负责将消息切割下来。由于这时候我们尚处在进行[IO](io.md)的fiber之中，通常我们会避免在此时进行解析以提高并行度。考虑到我们使用[**复制成本较低**的非连续缓冲区](io.md)，因此这儿切割的成本通常不高。

之后框架会根据请求是否属于[流式RPC](streaming-rpc.md)类的接口，（普通RPC）创建独立的[fiber](fiber.md)进行解析（`StreamProtocol::TryParse）处理，或（流式RPC）将之传递给这个流所关联的fiber进行解析和处理。

*我们的协议解析是在新创建的fiber进行的，因此不会阻塞其余请求的解析与处理。*

对于服务端，还涉及到服务端负责将消息分发给业务逻辑的类（[`StreamService`](../rpc/protocol/stream_service.h)的子类）。

由于我们无法提前预知由`StreamProtocol::TryParse`解析出的消息具体的C++类型，因此我们会尝试所有的`StreamService`进行处理。`StreamService`内部会通过[RTTI](https://en.wikipedia.org/wiki/Run-time_type_information)等方式尝试识别消息，如果无法识别，则由框架继续尝试其他`StreamService`。

*作为实现细节：我们在调用`StreamService`之前会使用`StreamProtocol`提供的工厂方法生成[`Controller`](../rpc/protocol/controller.h)的子类，用于传递上下文。*

考虑到同一连接上出现协议变化的情况较少，我们会记录上一次识别、处理请求的`StreamProtocol`及`StreamService`，并在下次优先尝试。这样我们可以消除掉为支持多种协议引入的尝试成本，使得性能和不支持多协议时基本持平。

服务端flare框架关注的是最终面对框架的`StreamProtocol`、`Message`、`Controller`等*接口*，而不关注其是否使用相同的*C++类型*。因此，**对于不同的协议可以直接提供支持而不需要翻译成某种统一的C++消息类型。**

### 客户端

与服务端不同，客户端不存在识别协议的需要，其协议已经在用户发起RPC时通过URI指定（或由具体的`Xxx_Stub`确定）了。

对于客户端而言协议的解析同服务端，与之不同的是，如果协议对象报“无法识别”的错误，我们不会尝试其他协议，而是直接作为错误向上层（业务代码）反馈。

客户端通常除了协议本身，还涉及到下面这些类：

- [`XxxChannel`](channel.md)：代表一个逻辑上的至某个服务（或集群）的连接
- `XxxController`：用于控制某次RPC的特定行为
- `XxxStub`：作为发起RPC的客户端的“代理”

不同的协议对这三种类可能有不同的实现，也可能采用不同的命名。某些情况下也可能会将一个或多个类合并或省略。flare框架对此没有作出限制。

以Protocol Buffers的场景举例（对应`XxxService_Stub`、[`RpcChannel`](../rpc/rpc_channel.h)、[`RpcClientController`](../rpc/rpc_client_controller.h)）：

- `XxxService_Stub`通常由[我们的插件](pb-plugin.md)生成，并提供各个RPC接口给业务使用。这种`XxxService_Stub`构造时接受[`google::protobuf::RpcChannel`](https://developers.google.com/protocol-buffers/docs/reference/cpp/google.protobuf.service)的子类，并通过`RpcChannel::CallMethod`收发请求。
- `RpcChannel`对象在`Open`时解析URI中的协议、解析服务名、发起连接到某一个服务实例，以便于后续`CallMethod`时进行网络IO。在`CallMethod`时`RpcChannel`还会构造上下文信息（`Controller`的子类，某些协议可能直接将`XxxController`继承于`Controller`而避免此处生成新对象）以便于传递[OOB信息](https://en.wikipedia.org/wiki/Out-of-band_data)给底层的协议（`StreamProtocol`）供其在解包、打包时使用。
- `RpcClientController`中保存的用户的选项将会被`RpcChannel::CallMethod`传递给框架。

简单来说，在客户端，flare框架只关注最终面对框架的`Controller`、`Message`的*接口*，而不关注其是否使用相同的*C++类型*。因此，**对于不同的协议可以直接提供支持，而不需要翻译成某中统一的C++消息类型。**

---
[返回目录](README.md)
