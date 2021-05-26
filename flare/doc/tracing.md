# 调用追踪

[分布式调用追踪](https://research.google/pubs/pub36356/)可以通过对服务调用链的跟踪，构建一个从服务请求开始到各个服务交互的全部调用过程的视图。用户可以从中了解到诸如应用调用的时延，网络调用的生命周期，系统的性能瓶颈等等信息。
Flare内置了对调用追踪逻辑的支持，通过不同的内置或第三方[provider](../rpc/tracing/tracing_ops_provider.h)，可以将RPC调用情况上报至相应的管理平台。

*我们只支持符合[OpenTracing](https://opentracing.io/)规范的调用追踪系统。*

显然，由于*各服务只上报自己的信息*的天性，为了能将整个调用链路串接起来，需要整个调用链条都使用相同的调用追踪平台才能获得有意义的结果。

目前我们内置了对如下平台的支持：

- (仅内部平台)

为了实现调用追踪，框架需要底层[传输协议](protocol.md)的支持（以便传递追踪上下文）。目前对于RPC而言，我们支持如下协议上的调用追踪：

- `flare`协议，对应的URI为`flare://...`；
- （实验性质，未完整测试）`trpc`协议，对应的URI为`trpc://...`。

*目前我们不支持[流式RPC](streaming-rpc.md)的调用追踪。*

## 使用

尽管我们尽可能的避免在关键逻辑上引入过多的调用追踪相关逻辑，取决于具体服务中业务逻辑的资源开销占比，**开启调用追踪仍然*可能*会导致明显的性能下降**。因此，在线上环境启用之前应当先进行适当的性能评估。

在发生**调用失败时，我们会有选择（概率性采样）的上报失败的调用链**，这可能是整个调用链路中的一部分，因此**可能会看到不完整的调用链**。

### 启用调用追踪

为了使服务会上报调用链信息，需要视情况开启如下开关：

- `--flare_tracing_provider=...`：这一开关指定了使用的调用追踪管理平台。目前的可选项有：
  - (仅支持内部系统)
- `--flare_rpc_start_new_trace_on_missing`：对于调用链路最顶端的服务，还需要额外指定这个`bool`型参数。当这一参数被指定时，如果请求方没有携带调用追踪的上下文，则会（概率性的采样）创建一个新的调用链。对于下游服务，其通常遵照上游传下来的采样决策决定是否上报，而不应当指定这一参数，否则可能会出现不完整的（由下游自行创建的）调用链。

另外，部分监控平台上报数据时需要提供上报方的服务名用于展示，因此我们建议服务实现方在初始化时填充[`Server::Options::service_name`](../rpc/server.h)字段来达到此目的。

### 记录业务特有信息

如果业务需要传递额外的数据以方便调试（非必须），可以通过[`RpcServerController`](../rpc/rpc_server_controller.h)的`AddTracingLog(...)`和`SetTracingTag(...)`来记录业务数据，这些数据之后将会被上报到调用追踪平台。

*部分追踪平台（如天机阁）会忽略`AddTracingLog(key, value)`时传入的`key`，这种情况下可以将所有的信息写在`value`中，并使用任意字符串作为`key`。*

*如果多次使用相同的`key`调用`SetTracingTag(key, value)`，那么只有最后一次调用的`value`会生效，之前的调用会被覆盖。`AddTracingLog(...)`则全都会上报。*

### 程序代码修改

如果业务代码遵照了我们的[开发建议](intro-rpc.md)，包括但不限于下列要求，那么**业务代码只需要按照上文描述开启相关开关即可，不需要针对RPC调用追踪开发逻辑**。

- 可行的情况下在`XxxServiceImpl::XxxMethod`中直接实现业务逻辑并同步完成；
  - 调用后端时同步接口及基于`Future<...>`的`Stub`接口均可。
- 需要并发操作时通过[`fiber::Async`](../fiber/async.h)创建新的[fiber](fiber.md)来进行；
- 其余情况下通过`Future<T>` + `fiber::BlockingGet(...)`等待其他任意环境（包括非flare的pthread环境）中的操作完成并获取返回值（这实质上在fiber中进行了*不阻塞pthread的*同步等待因此和第一条实际上类似）。

对于其他情况，请参考后文描述并在代码中自行传递调用链上下文。

## 程序内部上下文传递

内部而言，我们通过[fiber运行时](../fiber)中的[`ExecutionContext`](../fiber/execution_context.h)隐式的进行了一次RPC相关的上下文传递。对于调用追踪而言，这儿的上下文对应于[`SessionContext::tracing`](../rpc/internal/session_context.h)。

我们在调用业务的`XxxServiceImpl::XxxMethod`之前，会创建对应的`ExecutionContext`（下称“执行上下文”）并进行初始化，并在这个执行上下文内部调用业务代码。因此业务代码中任何同步的其他函数调用（如`XxxService_SyncStub::XxxMethod`发起RPC）中，框架均能够识别其所属的RPC上下文（绑定到了这个执行上下文），并增加必要的调用追踪逻辑。

同时，fiber运行时中，[`Async`](../fiber/async.h)、[`SetTimer`](../fiber/timer.h)（性能较差，不建议大量使用）内部均会捕获、并在调用业务代码回调之前，恢复执行上下文。因此，使用这些方法进行异步操作时，框架同样可以自动识别其所属的RPC并增加必要的调用追踪逻辑。

RPC完成时调用`done`（如果用户提供了）或调用传递给`Future<...>::Then`的`continuation`（假设未额外指定[`Executor`](../base/future/executor.h)）时框架已经设置好了执行上下文。

对于特殊的无法使用框架上述方法的业务逻辑而言，也可以通过如下方法自行捕获、恢复执行上下文以便使用RPC追踪（及Flare中其他依赖执行上下文的）能力：

- 在离开`XxxServiceImpl::XxxMethod`之前通过[`fiber::ExecutionContext::Capture()`](../fiber/execution_context.h)捕获执行上下文。
- 如果需要调用Flare的相关方法，通过`ExecutionContext::Execute(...)`来执行，这个方法会在执行回调（这个方法的参数）之前和之后分别启用/恢复之前捕获的执行上下文。
  - `ExecutionContext::Execute(...)`必须在fiber环境中执行，如果当前不属于fiber环境，则可以通过`flare::BlockingGet(fiber::Async(...))`来实现（注意这儿用的是pthread版本的`BlockingGet`，因为此时不属于fiber环境）。

内部而言，`Fiber::Attributes`同样允许创建fiber时指定其所属的执行上下文，但是考虑到这一类相对底层，**我们不推荐普通用户直接使用`Fiber`类。**

---
[返回目录](README.md)
