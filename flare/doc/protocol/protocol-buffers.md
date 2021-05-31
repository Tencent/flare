# Protocol Buffers

我们以[Protocol Buffers](https://developers.google.com/protocol-buffers)作为我们主要的RPC协议。

我们提供了如下专用于Protocol Buffers协议的类用于进行基于这一协议的RPC开发：

- [`RpcServerController`](../../rpc/rpc_server_controller.h)：用于服务端控制单次RPC的行为。
- [`RpcClientController`](../../rpc/rpc_client_controller.h)：用于客户端控制单次RPC的行为。
- [`RpcChannel`](../../rpc/rpc_channel.h)：用于[描述至某个集群的虚拟链路](../channel.md)。
- 由[flare的pb插件](../pb-plugin.md)生成的`SyncXxxService`（服务端）或`XxxService_SyncStub`（客户端）类。

这篇文档描述了这些类的基本行为以及各基于Protocol Buffers的[线上格式（Wire protocol）](https://en.wikipedia.org/wiki/Wire_protocol)。

*关于如何编写基本的Protocol Buffers的服务端、客户端，可参考[导引](../intro-rpc.md)，此处不再赘述。*

## C++接口

这一节介绍了语言层面我们提供的一些Protocol Buffers特有的类。

### `RpcServerController`

`RpcServerController`用于服务端控制单次RPC，包括但不限于设置错误码（`SetFailed`）等。

关于这个类的具体接口可以参考类中注释。

### `RpcClientController`

`RpcClientController`用于客户端控制单次RPC，包括但不限于设定超时（`SetTimeout`）、重试（`SetMaxRetries`)、RPC失败时获取错误信息（`ErrorText`）等。

关于这个类的具体接口可以参考类中注释。

### `RpcChannel`

`RpcChannel`只用于RPC客户端（主调方），用于描述连接到某个集群的虚拟链路。

为了打开一个虚拟链路，用户需要通过两种方式之一完成：

- 构造`RpcChannel`时传入需要请求的集群的URI。
- 默认构造`RpcChannel`，并通过`RpcChannel::Open`打开相应的URI。

无论使用何种方式，如果打开失败，后续通过这个`RpcChannel`进行的RPC均会以[`rpc::STATUS_INVALID_CHANNEL`](../../rpc/protocol/protobuf/rpc_meta.proto)失败。

#### URI 格式

`RpcChannel::Open`接收一个URI指定目标集群及其协议，格式形如`协议://集群名`。

`协议`通常是一个不含`+`的字符串（如`flare`），或是由某种底层协议承载的上层协议，如`http+pb`。

另外，`协议`暗含了其后的`集群名`所使用的格式。这一行为*大体*符合[RFC 3986对`authority`中`host`含义的表达](https://tools.ietf.org/html/rfc3986#section-3.2.2)：

> ... A host identified by a registered name is a sequence of characters usually intended for lookup within a locally defined host or service name registry, **though the URI's scheme-specific semantics may require that a specific registry (or fixed name table) be used instead.**

*取决于具体的[NSLB](../nslb.md)，RFC 3986中关于大小写敏感的要求我们可能无法满足。*

对于需要使用非默认NSLB的场景，`RpcChannel`允许通过`RpcChannel::Options::override_nslb`来指定不同的名字服务及负载均衡策略，如：

```cpp
RpcChannel channel;
auto success = channel.Open("flare://127.0.0.1:1234",
                            // `list+rr`:
                            //
                            // Use `list` (a list of address) as name resolver,
                            // use `rr` (round robin) as load balancer.
                            RpcChannel::Options{.override_nslb = "list+rr"});
if (success) {
  // ...
}
```

**在无歧义的情况下，我们会尽可能的自动识别`集群名`的格式**。如上述`flare:://127.0.0.1:1234`，即便不指定`override_nslb`，我们也会使用`list+rr`（IP列表、round-robin）来解析。这是因为尽管`flare`协议默认使用Polaris，但是`127.0.0.1:1234`并不符合Polaris的集群名的命名规范（包含了`:`）。因此，只有当flare确实无法正常识别`集群名`格式时，才需要手动指定这一参数。

每个协议的默认NSLB可能不同，具体可以参考各个协议的文档（见后文）。

### `XxxService_(Sync|Async)Stub`

最终进行RPC通常是通过某种类型的`Stub`进行，关于`Stub`的生成及使用，可以参考[我们的Protocol Buffers Plugin的文档](../pb-plugin.md)。

### 高级技巧

这一节列出了一些平时较为少用的高级技巧。

#### 提前返回响应

通常而言我们会在`XxxServiceImpl::XxxMethod`返回之后将`response`序列化并返回。

对于部分延迟敏感的服务，如果`XxxMethod`在填充`response`之后还有一些不影响`response`的逻辑需要执行（如清理逻辑），可能会希望在执行这些逻辑之前先对请求方返回数据。

对此我们提供了如下方法：

- [`RpcServerController::WriteResponseImmediately()`](../../rpc/rpc_server_controller.h)：调用这个方法会使框架**立即对`response`中的数据进行序列化并返回给调用方**。需要注意的是，一旦这个方法被调用，服务实现方**后续不能再操作`response`及`controller`**，否则行为是未定义的。

#### 手动序列化/反序列化请求/响应

考虑到Protocol Buffers的编解码效率，对于消息较为复杂的场景，避免不必要的序列化/反序列化可能会有较为明显的性能收益。

因此，在某些特殊场景中（如编写代理类型的服务），可能会存在程序本身已经持有已经序列化后的请求/响应、或程序本身期望直接接收未被反序列化的请求/响应。

*如果使用本节介绍的技巧，服务开发人员需要自行保证二进制消息和实际的消息类型相匹配。（如果编译时没有定义`NDEBUG`（即调试版），那么编译产出中框架会进行必要的检查。但是考虑到性能问题，用于发布的版本中（编译时定义了`NDEBUG`）我们会将至条件编译掉。）*

Flare对这些场景提供了一定的支持，具体可以参见如下方法/属性：

- [`RpcClientController`](../../rpc/rpc_client_controller.h)：
  - `SetRequestRawBytes`：如果在发起RPC之前，通过这一方法提供了二进制字节流，那么框架将会忽略传给`XxxService_Stub`的`request`，并直接使用这儿提供的二进制作为*已经序列化*的消息。
  - `SetAcceptResponseRawBytes`、`GetResponseRawBytes`：如果在发起RPC之前调用了`SetAcceptResponseRawBytes`，那么RPC完成之后收到的*未反序列化*（且**未检查合法性**）的响应会被放在`GetResponseRawBytes`中。同时，传递给`XxxService_Stub`的`response`会被忽略。
- [`RpcServerController`](../../rpc/rpc_server_controller.h)：
  - `GetRequestRawBytes`：如果RPC的方法被`accept_request_raw_bytes`（见后文）修饰了，那么框架在收到这一方法的请求时不会对消息进行反序列化（且**不会进行合法性检查**），而是直接将之通过`GetRequestRawBytes`传递给服务实现方。这种场景下`XxxServiceImpl::XxxMethod`收到的`request`没有意义。
  - `SetResponseRawBytes`：如果在RPC返回之前通过这一方法提供了一个二进制字节流，那么这儿二进制会直接作为*已序列化*的消息返回，同时，任何对`response`的修改将会被丢弃。
- [`accept_request_raw_bytes`](../../rpc/rpc_options.proto)：
  - 这个Protocol Buffers的`option`用于修饰`.proto`中定义的方法。如果指定了，那么框架不会进行请求的反序列话（或合法性检查），而是直接将原始的二进制通过`RpcServerController::GetRequestRawBytes`传递给服务实现方。

#### 压缩/解压缩

可以通过`RpcController`对请求或响应的消息`Body`进行压缩/解压缩。

- [`RpcClientController`](../../rpc/rpc_client_controller.h)：
  - `SetCompressionAlgorithm`：设置压缩算法对请求的消息主体进行压缩。

- [`RpcServerController`](../../rpc/rpc_server_controller.h)：
  - `EnableCompression`: 服务端开启压缩, 对响应的消息主体进行压缩, 框架会在协议支持的压缩算法中根据预先定义好的优先级挑选一个压缩算法。

## IDL扩展选项

我们提供了一些[扩展选项](../../rpc/rpc_options.proto)用于指导/修改框架的行为。

### 服务扩展选项

- `flare.xxx_service_id`：根据协议不同，请参考后文中具体的协议描述的章节。

### 方法扩展选项

- `flare.xxx_method_id`：根据协议不同，请参考后文中具体的协议描述的章节。
- `flare.max_queueing_delay`：指定请求的最大排队时长，单位毫秒。如果设置且不为0，那么当系统负载较高时，如果这个方法的请求在系统内排队时间超过了这个限制，那么框架会对这个请求直接返回`rpc::STATUS_OVERLOADED`，不会调用具体的方法实现。
- `flare.max_ongoing_requests`：指定这一方法最大并发度（同时在被处理的请求个数）。如果设置且不为0，在达到这一上限时，新收到的请求会直接返回`rpc::STATUS_OVERLOADED`。

*上述过载控制的方法实际返回的状态码受`--flare_rpc_protocol_buffers_status_code_for_overloaded`参数控制，默认为`rpc::STATUS_OVERLOADED`，但可以通过这一参数修改。*

## 线上格式

这一节列出了我们内置提供支持的各种基于Protocol Buffers的协议。

### flare

这一协议在URI中使用`flare`标识。

*Polaris的地址默认使用Production命名空间解析，要显式指定命名空间可以使用形如`flare:://example.service@Test`的格式。其他使用Polaris的协议也相同。*

[flare协议](../../rpc/protocol/protobuf/std_protocol.h)的线上格式如下：

```text
[Header][meta][payload][attachment]
```

其中`Header`使用如下C++定义：

```cpp
struct Header {
  std::uint32_t magic;
  std::uint32_t meta_size;
  std::uint32_t msg_size;
  std::uint32_t att_size;
};
```

考虑到我们通常所使用的机器均使用[小尾格式](https://en.wikipedia.org/wiki/Endianness#Little-endian)（x86-64、ARMv8，严格来说ARMv8同时支持大尾和小尾，但是通常使用小尾），因此上述字段字节序我们均采用小尾（而**非网络序**）编码。

其中：

- `magic`：定义为（C++语法）`'F' << 24 | 'R' << 16 | 'P' << 8 | 'C'`。
- `meta_size`：`meta`的字节长度。
- `msg_size`：`payload`的字节长度。
- `att_size`：`attachment`的字节长度。

`meta`为[`RpcMeta`](../../rpc/protocol/protobuf/rpc_meta.proto)的序列化后的二进制形式，`payload`为具体消息正文的二进制表示（可能为空），`attachment`为业务自行指定的额外不透明二进制数据（可能为空）。

#### 流式RPC

流式RPC与普通RPC格式相同，通过将`RpcMeta::method_type`指定为`METHOD_TYPE_STREAM`标识相应的请求或响应为流式请求/响应。

flare协议对[服务端流式RPC、客户端流式RPC、双向流式RPC](../streaming-rpc.md)均提供支持。

### HTTP承载的Protocol Buffers

目前广告线内部使用最多的是通过HTTP承载的Protocol Buffers。

这一协议在URI中使用`http+pb`标识（也可使用别名`http`），其默认的NSLB为`cl5`。如：`http://l5:123-456`。

*由于HTTP协议解析的复杂程度，这一协议性能表现相对较差。（但是由于我们在[调度逻辑](../fiber-scheduling.md)上的优化，其Echo压测的性能（QPS、延迟、毛刺）表现仍然**可感知的好于老的RPC框架，并大幅度领先于老的RPC框架+线程池**）。*

这一协议不支持附件、RPC调用追踪。流式RPC的支持情况见下文描述。

#### 普通RPC

对于普通RPC，我们将Protocol Buffers编码后的数据通过HTTP消息的正文传输。其中编码方式可能有多种，详见后文描述。

请求中会增加如下headers：

```http
Rpc-SeqNo: 123
Content-Type: application/x-protobuf
```

其中：

- `Rpc-SeqNo`：用于关联请求和响应，以实现同一连接上同时处理多个请求。（HTTP1.1本身不支持响应和请求乱序，只通过[HTTP pipelining](https://en.wikipedia.org/wiki/HTTP_pipelining)对复用提供了有限的支持，我们这儿的行为严格来说不合标准。）

  *特别的，我们允许请求方不填充这一字段。但是这种场景下，请求方不应同时在连接上发起多个请求（否则无法正确关联请求响应）。*

- `Content-Type`，其有如下几种取值，决定了HTTP消息正文的编码方式，服务端通常会用相同编码方式返回响应。
  - `application/json`：我们自研的pb to json实现序列化出的JSON文本，其中字段名保持原始格式（通常是snake_case）。

    这个`Content-Type`对应于`http+gdt-json`协议。

  - `application/x-proto3-json`：通过[Protocol Buffers官方库进行的JSON序列化](https://developers.google.com/protocol-buffers/docs/proto3#json)，其中字段名会被改为lowerCamelCase。（注：此处`proto3`指的是pb运行时为3.0+，对于`proto2`语法的`.proto`描述文件，也支持此种编码。）

    对应`http+proto3-json`协议。

  - `application/x-protobuf`：通过[`google::protobuf::Message::SerializeAsString()`](https://developers.google.com/protocol-buffers/docs/cpptutorial#parsing-and-serialization)进行序列化。

    对应`http+pb`协议。

  - `text/x-protobuf`：通过`google::protobuf::Message::ShortDebugString()`进行序列化。

    对应`http+pb-text`协议。

- `Rpc-Timeout`：[请求方期望的超时](https://grpc.io/docs/guides/concepts/#deadlines-timeouts)，单位毫秒。若在给定超时内无法完成处理，则请求方会丢弃结果，这可用于服务端优化其实现（如根据时间预算控制计算量提供*尽可能*精确的结果）。

响应时根据成功或失败，会返回如下状态码：

- 200：RPC成功
- 404：请求的方法未找到
- 500：其他错误

响应中会增加如下headers：

```http
Rpc-SeqNo: 123
Content-Type: application/x-protobuf
Rpc-Error-Code: 99
Rpc-Error-Reason: Error description.
```

其中：

- `Rpc-SeqNo`：用于多个请求共享一个链接时关联请求与响应。
- `Content-Type`：同请求中对应的头。
- `Rpc-Error-Code`：错误码，定义参见[rpc_meta.proto](../../rpc/protobuf/rpc_meta.proto)，可以通过`RpcServerController`设置，`RpcClientController`访问。
- `Rpc-Error-Reason`：错误的文本描述。可以通过`RpcServerController`设置、`RpcClientController`访问。

##### 示例

此处给出使用第三方客户端[`curl`](https://curl.haxx.se/)直接通过HTTP接口请求[`example/rpc/server.cc`](../../example/rpc/server.cc)的示例：

```bash
# `Content-Type` must be set correctly. `Rpc-SeqNo` must be set to a numeric
# value.
#
# Meanwhile, server side must have already enabled protocol `http+proto3-json`.
$ curl localhost:5567/rpc/example.protobuf_echo.EchoService.Echo -H'Content-Type: application/x-proto3-json' -H'Rpc-SeqNo: 1' -XPOST -d'{"body": "Hello world."}' -vv
* About to connect() to localhost port 5567 (#0)
*   Trying 127.0.0.1...
* Connected to localhost (127.0.0.1) port 5567 (#0)
> POST /rpc/example.protobuf_echo.EchoService.Echo HTTP/1.1
> User-Agent: curl/7.29.0
> Host: localhost:5567
> Accept: */*
> Content-Type: application/x-proto3-json
> Rpc-SeqNo: 1
> Content-Length: 24
>
* upload completely sent off: 24 out of 24 bytes
< HTTP/1.1 200 OK
< Rpc-SeqNo: 1
< Content-Type: application/x-proto3-json
< Content-Length: 23
<
* Connection #0 to host localhost left intact
{"body":"Hello world."}
```

需要注意的是，为了使用`application/x-proto3-json`方式请求服务端，需要启用`http+proto3-json`协议。

#### 流式RPC

这一线上格式只支持流式返回（一问多答），但不支持流式请求（多问一答）或双向流式（多问多答）的情况。

流式RPC我们通过[`chunked`编码](https://en.wikipedia.org/wiki/Chunked_transfer_encoding)实现。

每次流式RPC均会独占使用一个连接，完成后连接即关闭。

请求格式同普通RPC，不再赘述。

响应始终返回HTTP 200，流式RPC的状态码不通过HTTP状态码标识，具体表示方式参见后文。

另外，响应头中会增加如下headers：

```http
Trailer: Rpc-Error-Code
Transfer-Encoding: chunked
Rpc-SeqNo: 123
```

其中：

- `Transfer-Encoding`标记了使用`chunked`编码（因此是一个流式RPC响应）。
- `Rpc-SeqNo`为这一协议的[自定义头](https://tools.ietf.org/html/rfc6648)，用于关联请求和响应。我们使用64位整数。
- `Trailer`标记了最后本次`chunked`编码传输尾部的[trailing header](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Trailer)，我们将通过这个字段返回此次RPC的状态码。

之后，对于此次流式RPC，每个响应包对应一个chunk，其使用Protocol Buffers的二进制格式编码。

下面是一个线上格式的示例：

```http
HTTP/1.1 200 OK
Trailer: Rpc-Error-Code
Transfer-Encoding: chunked
Rpc-SeqNo: 131073

00000005
\x**\x**\x**\x**\x**
00000005
\x**\x**\x**\x**\x**
00000005
\x**\x**\x**\x**\x**
00000005
\x**\x**\x**\x**\x**
0
Rpc-Error-Code: success
```

其中`\x**\x**\x**\x**\x**`表示5字节的二进制数据（Protocol Buffers编码后的消息）。

### QZone协议承载的Protocol Buffers

这一协议在URI中使用`qzone-pb`标识（也可使用别名`qzone`），其默认的NSLB为`cl5`。如：`qzone://l5:123-456`。

QZone协议的线上格式可[参阅代码](../../rpc/protocol/protobuf/qzone_protocol.cc)，此处不作单独描述。

由于QZone协议本身并未提供通过字符串形式标记被调方法的能力，因此我们要求业务在`*.proto`中通过[`flare/rpc/rpc_options.proto`](../../rpc/rpc_options.proto)中的`flare.qzone_service_id`、`flare.qzone_method_id`来标识方法对应的QZone协议中的`version`、`cmd`，如：

*由于历史原因，Flare也支持广点通代码库中之前使用的`gdt.gdt.qzone_protocol_version`、`gdt.qzone_protocol_cmd`，二者效果相同。但使用`qzone_service_id`、`qzone_method_id`更能明确的体现这些ID的逻辑层面上的含义。*

```protobuf
syntax = "proto3";

import "flare/rpc/rpc_options.proto";

// ... (Definition of `EchoRequest` & `EchoResponse`.)

service EchoService {
  option (flare.qzone_service_id) = 2;
  rpc Echo(EchoRequest) returns(EchoResponse) {
    option (flare.qzone_method_id) = 1001;
  }
}

```

需要注意的是，QZone协议用于关联请求及响应的字段为*32位*整数。我们的实现中对*32位*的`correlation_id`的生成性能可能不及64位`correlation_id`，这可能导致一定的性能损失。

**对于流式RPC，这一格式只能提供有限的支持**。另外，由于QZone协议无法传递end-of-stream标记，因此需要服务实现方自行通过响应消息等方式提前通知对端实际的响应个数（而不能连续读取直到`StreamReader::Read`返回`EndOfStream`。对于新的服务或接口，我们推荐通过flare协议提供流式RPC的能力。

### baidu_std (brpc)

这一协议在URI中使用`baidu-std`标识，默认的NSLB为Polaris（因为我们没有[BNS](https://github.com/apache/incubator-brpc/blob/master/src/brpc/policy/baidu_naming_service.cpp)）。如`baidu-std://example.service`。

brpc的协议格式可参考[brpc文档](https://github.com/apache/incubator-brpc/blob/master/docs/cn/baidu_std.md)。

对端如果使用brpc开发，需要启用`baidu_protocol_use_fullname`（brpc默认启用）。

目前我们的实现尚不支持流式RPC、RPC调用追踪。

### poppy

Poppy协议在URI中使用`poppy`标识，默认的NSLB为`list+rr`。如：`poppy://192.0.2.1:5678`。

简单来说，Poppy首先使用HTTP协议进行握手，随后按照自有的二进制协议进行通信。因此，对于长连接的场景，Poppy协议和其他二进制协议类似，具有不错的性能表现。

Poppy协议不支持流式RPC、附件、RPC调用追踪等。
