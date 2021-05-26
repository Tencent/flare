# 调试

我们相信，调试体验也是一个框架很重要的一部分。

## rpc

我们通过`/inspect/...`、`/prof`等地址对外暴露了一系列基于HTTP协议的调试接口：

- [`/inspect/version`、`/inspect/status`](../rpc/protocol/http/builtin/misc_handler.h)：这两个接口对外提供了一些程序的基本信息（如CVS版本、启动时间等）。
- [`/inspect/rpc_stats`](../rpc/protocol/http/builtin/rpc_statistics_handler.h)：这一接口可以用于查询RPC统计。尽管其输出已经是JSON格式了，我们也另外提供了[脚本用于解析其输出](../tools/rpc_stat.py)。
- [`/inspect/vars`](../rpc/protocol/http/builtin/exposed_vars_handler.h)：这一接口可以用于查询程序内通过[`ExposedXxx`](../base/exposed_var.h)对外暴露的各类内部统计/性能信息。这通常可以用于细粒度的性能调试。
- [`/inspect/gflags`](../rpc/protocol/http/builtin/gflags_handler.h)：这一接口可以用于查询或修改GFlags。
- [`/inspect/options`](../rpc/protocol/http/builtin/options_handler.h)：这一接口用于查询[配置中心](option.md)相关的内部信息，如用于检查程序内部是否已经同步到了配置中心的最新值。
- [`/inspect/rpc`](../rpc/protocol/http/builtin/rpc_form_handler.h)：这一接口允许用户通过浏览器发起RPC。
- [`/inspect/rpc/reflect`](../rpc/protocol/http/builtin/rpc_reflect_handler.h)：这一接口提供了一定程度的“RPC反射”的能力，用于查询程序内部对外提供的服务。
- `/prof/...`：`/prof/...`一系列接口提供了在线性能/内存分析的能力，具体可以参考[其文档](profiler.md)。

**出于安全考虑，上述接口不应当对外网暴露**。如果服务需要对外网暴露，通常我们推荐在对外的反向代理（如nginx）上过滤相关的URI前缀。

*如果修改反代配置不方便，也可以通过[`Server::Options::no_builtin_pages`](../rpc/server.h)统一关闭，但是这会让整个程序成为“黑盒”，我们通常不推荐这么做。*

另外。如果对于内网也有过滤需求，可以通过使用[`HttpFilter`](../rpc/http_filter.h)的方式，如通过[注册某些鉴权的`HttpFilter`](intro-http.md)来过滤未授权的用户对上述调试接口的访问。**需要注意的是这可能会令通过`curl`等命令行方式调试进程变得困难。**

## fiber

目前基于用户态调度的框架在调试体验上普遍较为欠缺，这儿列出了一些常见的问题。

### 枚举挂起的fibers

*对于协程类框架，同义问题为“枚举挂起的协程”，技术上属于相同问题。*

目前协程类框架多未提供此种能力，因此对于未知原因导致挂起的上下文难以调试。（2019年末状况）

我们提供了GDB插件，引入了新的`list-fibers`命令，可以用于枚举所有当前未在执行状态的fibers。具体使用及技术细节可见[gdb-plugin.md](gdb-plugin.md)。

*brpc也提供了类似的[GDB插件](https://github.com/apache/incubator-brpc/blob/master/tools/gdb_bthread_stack.py)。*

### Sanitizers

[Sanitizers](https://github.com/google/sanitizers)对于追踪代码中崩溃时不在现场的bug往往有很好的效果。但是对于基于用户态调度（无论是有栈协程还是fiber）的库而言，直接使用sanitizers往往会以程序崩溃告终。

目前无论协程类或是基于fiber类的框架，多未提供此种能力。（2019年末状况）

flare有必要的和sanitizers之间的协作，目前[对sanitizers提供了一定的支持](sanitizers.md)。

---
[返回目录](README.md)
