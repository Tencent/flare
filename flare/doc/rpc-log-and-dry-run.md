# RPC流量保存及回放

考虑到对多种不同平台需要/提供的接口及功能不尽相同，我们最终设计了一套[与具体实现无关的接口](../rpc/binlog)以期望可以支持多种不同的平台。

我们将这一个大功能拆分成如下两部分：

- [RPC流量的录制](../rpc/binlog/dumper.h)。这一步录制至磁盘或第三方网络平台的数据类似于[binlog](https://dev.mysql.com/doc/refman/8.0/en/binary-log.html)。
- [利用录制的流量模拟正常运行环境](../rpc/binlog/dry_runner.h)以支持测试（即“回放”，类似于[Dry run](https://en.wikipedia.org/wiki/Dry_run_(testing))）。

目前我们内置了如下几种支持，另外也可以参考[内置实现](../rpc/binlog/)来自行通过我们的接口对Flare进行扩展。

- 纯文本（调试用途）。
- 只记录服务本身收到的请求及返回的响应（不涉及重放）。
- (其他内部平台)

## 录制流量

要启用录制流量的能力，需要通过参数`--flare_binlog_dumper=`指定需要使用哪个平台/系统来录制流量。这通常和之后进行回放时使用的平台/系统相同。

在接收到一个请求时，取决于是否命中采样，我们会在[服务端](../rpc/internal/normal_connection_handler.cc)处录制和这个请求关联的所有收发过的数据包。与此同时，类似于[RPC追踪](tracing.md)的设计，我们会通过和业务代码执行环境相关联的局部存储（[`ExecutionLocal`](../fiber/execution_context.h)）来标记这次请求。

相对于直接录制二进制数据，我们选择了在能够感知具体协议的这一层来录制。当然这意味着录制服务端RPC请求/响应需要具体协议的实现方的支持，如[Protocol Buffers的`Service`实现](../rpc/protocol/protobuf/service.h)。但是实际上考虑到部分协议单纯录制每个包对应的二进制数据可能会导致信息缺失（如HBase握手阶段有“被调用的服务名”的关键信息），因此在感知协议的这一层进行捕获是一个更好的选择。

后续当业务代码通过*对回放感知*的各种客户端（RPC、HTTP等）请求外界时，如果我们检测到这次请求的流量需要录制，那么这些客户端会对用户实际收发过的包进行录制。

**这也意味着在录制/回放时，如果使用了不支持流量录制的客户端、或第三方客户端时，使用这些客户端所执行的操作，依然会对外界产生可感知的副作用。**

另外尽管“录制流量”本身并不需要，在录制过程中，我们提供了包括但不限于`RpcServerController::SetBinlogContext(...)`、`RpcClientController::SetBinlogCorrelationId(...)`在内的途径用于帮助业务代码/框架在回放过程中实现*确定性*的行为（如使用相同随机数种子保证随机序列一致等）。

*框架在初始化录制流量的上下文时会生成一个`binlog_correlation_id`，但是如果服务逻辑请求了多个相同的后端（协议、Channel地址、方法名均相同），我们可能需要服务代码本身通过`SetBinlogCorrelationId`来对各个请求进行区分以便于我们后续在回放时关联响应。其余情况下通常框架可以自动填充必要的`binlog_correlation_id`。*

为了方便调试观察，我们不但会录制请求本身，在可能的情况下，还会额外录制一份数据包的字符串表达以便于对比。由于不同的流量录制平台展示需求不同，这个录制行为取决于具体的[`Dumper::InspectXxxPacket`](binlog/../../rpc/binlog/dumper.h)的实现方。

框架方面录制流量时需要指定如下参数：

- `--flare_binlog_dumper`：需要使用的记录流量的系统（可选项见后文）。留空则不启用相关能力。
- `--flare_binlog_dumper_sampling_interval`：记录两个请求之间的最短间隔（毫秒），内部实现而言这个参数实际的解析度大约在10ms级别，因此过小时不会继续提升录制速度。考虑到通常我们不需要短时间内录制大量请求，因此建议设置一个较大的值（如`1000`，即每秒1个）。

## 回放

显然在回放（通常即测试）时框架需要与正常运行时不同的逻辑对服务本身的逻辑提供服务，这体现在如下几个方面：

- 请求包可能使用了特有的格式（比如增加回放系统特有的包头或包装层），因此需要特殊的解析逻辑。
- 如果之前录制时服务本身通过`SetBinlogContext`保存了其上下文，我们需要将之：1) 提取出来，2) 通过`RpcServerController::GetBinlogContext(...)`将之交与服务代码。
- 调用后端时不应当请求原始后端，取决于具体的回放系统，这可能意味着我们需要使用包括但不限于如下方法之一的途径来模拟响应：
  - 从某个集中存储（磁盘上的数据、KV数据库等）按照`SetBinlogCorrelationId`时提供的关联ID查询响应结果。
  - （可能需要二次包装）将请求转发至特定的假后端（如我们现有的RPC回归逻辑）。

因此，在进行回放测试时，需要通过参数`--flare_binlog_dry_runner=`来告知框架*程序运行在回放环境*。

**我们不支持同时指定`--flare_binlog_dump`和`--flare_binlog_dry_runner`参数。这种组合也鲜有意义。**

显然，通常而言，录制流量和回放时应当使用相同的平台。

考虑到各个回访平台的内部数据的不同，我们需要针对各个回放平台分别提供[`DryRunner`、`DryRunContext`](../binlog/../rpc/binlog/dry_runner.h)的实现。

在启用回放时，收到请求之后服务端会：

- 将数据包交给[`DryRunner`](../rpc/binlog/dry_runner.h)进行处理并根据其私有协议解析出`DryRunContext`。一个`DryRunContext`对应一个RPC请求（无论是否是流式，一整个RPC对应一个`DryRunContext`）。
- 考虑到在我们录制流量时实际上是由具体的[`StreamService`](../rpc/protocol/stream_service.h)类进行的序列化，因此这儿我们序列化时录制的`StreamService`的UUID找到对应的对象并通过`ExtractCall`进行反序列化。
- 反序列化之后我们得到了必要的`Message` / `Controller`等对象之后，就可以如普通处理逻辑一样调用服务自身代码（但是会额外将`binlog_context`传递过去）。

在回放环境中，通过*对回放感知*的客户端请求外界时，相对于实际的请求外界，框架会转而通过调用`DryRunnerContext`相关的方法，使用回放系统之前录制的响应来返回结果（并根据`DryRunContext`的实现，可能会录制回放过程中发出的请求内容以便后期对比）。

最终框架在返回时，也会通过`DryRunner`对响应的二进制进行二次打包，返回给请求方。之后通常回放系统（的页面等）还会输出回放环境下服务的响应和之前录制流量时录制的响应的区别。

## 支持的协议

### 服务端

目前我们服务端支持如下协议：

- 各类基于Protocol Buffers的协议，包括但不限于`flare://`、`http://`（仅支持HTTP承载的pb）、`qzone://`（QZone承载的pb）、`baidu-std://`等。

  *我们目前不支持流式RPC、不支持不解析消息（`RpcXxxController::Xxx[Request|Response]Bytes*`）的用法。*

目前服务端尚不支持如下协议：

- HBase

### 客户端

目前我们客户端支持如下协议：

- 各类基于Protocol Buffers的协议。

  *我们目前不支持流式RPC、不支持不解析消息（`RpcXxxController::Xxx[Request|Response]Bytes*`）的用法。*

目前客户端尚不支持如下协议：

- HBase

## 支持的系统

这儿我们列出了我们现在支持的流量录制/回放系统。

### 纯文本

这一系统主要用于调试用途，可通过如下参数启用：

- `--flare_binlog_dumper=text_only`
- `--flare_binlog_text_only_dumper_filename=path/to/dump.txt`：这一参数指定了保存录制结果的文本文件。

---
[返回目录](README.md)
