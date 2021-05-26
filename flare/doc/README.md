# Flare 服务框架技术文档
## 使用

- [开始使用](getting-started.md)
- [RPC 入门导引](intro-rpc.md)
- [GFlags](gflags.md)
- [Channel](channel.md)
- [名字服务与负载均衡](nslb.md)
- [Controller](controller.md)
- [RPC 状态码](pb-rpc-status.md)
- [PB 插件](pb-plugin.md)
- [流式 RPC](streaming-rpc.md)
- [异步](async.md)
- [纤程](fiber.md)
- [Future](future.md)
- [定时器](timer.md)
- [HTTP](intro-http.md)
- [配置](option.md)
- [日志](logging.md)
- [测试](testing.md)
- [调试](debugging.md)
- [Profiler](profiler.md)
- [监控](monitoring.md)
- [表单提交](rpc-form.md)
- [流量保存及回放](rpc-log-and-dry-run.md)
- [调用追踪](tracing.md)

除了本文档外，[../example](../example)目录下还有很多示例代码。

## 原理

这部分讲述 flare 框架的内部实现原理，有助于你更好地理解框架，并进行扩展开发和参与协同。

- [IO](io.md)
- [协议](protocol.md)
- [调度组](scheduling-group.md)
- [Fiber 调度](fiber-scheduling.md)

## 开发与优化

- [性能指引](performance-guide.md)
- [性能测试](benchmark.md)
- [RTTI](rtti.md)
- [对象池](object-pool.md)
- [时间戳](timestamp.md)
- [Sanitizers](sanitizers.md)
