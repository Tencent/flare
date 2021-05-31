# Channel

Channel用于RPC主调方（客户端方），用来描述一个到特定服务（通常可能是一个*集群*，而不一定是某一台特定的服务实例）的虚拟链路。

由于网络的天生的不可靠性，Channel也会在用户（通常是通过类似于[`RpcClientController`](../rpc/rpc_client_controller.h)等类，视[协议](protocol.md)不同而不同）**显式指定**时，采取包括但不限于重试、[backup request](http://highscalability.com/blog/2012/6/18/google-on-latency-tolerant-systems-making-a-predictable-whol.html)等方式，来尽可能提高可靠性。

显而易见的，由于Channel的目标是*对一个集群（中的某个实例）进行RPC*，因此其需要：

- 采取某些方式将集群名映射到一个或多个服务实例的地址（IP:端口）以便进行网络IO
- 按照[线上格式（Wire protocol）](https://en.wikipedia.org/wiki/Wire_protocol)对请求进行编解码
- 必要的情况下采取某些提高RPC可靠性的措施（通常需要用户显式指定，如重试等。但需要注意[幂等问题](https://en.wikipedia.org/wiki/Idempotence)）

## 名字解析、负载均衡

对于Channel而言，发起RPC的第一步是将类似于`flare://example.Service`的[URI](https://tools.ietf.org/html/rfc3986)解析为一系列的网络地址（IP:端口），并从中选择一个或多个（启用备份请求的场景）进行RPC。

这儿需要Channel将URI按照`协议://服务名`的格式进行解析。

取决于[协议的具体实现](protocol.md)，`协议`字段可能是确定的，也可能是某种字符串（如[Protocol Buffers](protocol/protocol-buffers.md)。

不同的协议有不同的`服务名`的默认格式，取决于Channel的具体实现，可能会提供覆盖这一默认行为的方法。具体请参考各个协议的文档。

Channel通常会按照`服务名`的格式（无论是默认格式又或者是用户显式通过`Channel::Options`等方式指定的特定格式），通过以下两种方式之一得到一个或多个后端实例的地址：

- 通过[`NameResolver`](nslb.md)进行解析后通过[`LoadBalancer`](nslb.md)从中选择合适的后端实例
- 或者对于类似于CL5等整合了NSLB的负载均衡器，通过[`MessageDispatcher`](nslb.md)

之后Channel会负责发起RPC。

## 协议解、打包

取决于Channel的具体实现，其可能关联至特定的协议，又或是支持多种协议（如Protocol Buffers），对于后者的情况，通过修改URI中的`协议`段，调用方可以选择其认为可以被后端服务所理解的线上格式进行RPC。

关于协议的更多信息可以参考[protocol.md](protocol.md)。

---
[返回目录](README.md)
