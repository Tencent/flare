# Profiller

Flare 开发的服务支持在运行期间对程序进行[动态分析](https://en.wikipedia.org/wiki/Profiling_(computer_programming))，以发现程序的性能热点或者内存分配情况。

目前支持`CpuProfiler`以及`MemProfiler`来分别分析程序`cpu`和内存使用情况, 找到程序中的热点函数或可能的内存泄漏。

要使用`Profiler`程序需要先运行一个`flare::Server`, 如果只是客户端服务或没有使用`flare::Server`, 可以创建一个`dummy server`, 可以参考[例子](../example/rpc/press.cc)。
可以在程序运行中间动态启动`Profiler`, 得到函数调用图, 停止。
通过`http`的`Get`方法访问控制, 可以通过命令行`curl`调用, 也可通过浏览器直接访问查看`svg`格式的函数调用图。

`Profiler`的原理大致都是采样收集函数调用栈,得到汇总调用图。通过设置合理的采样频率, profiler对程序的性能影响不明显。

## Cpu Profiler

`flag::Server`默认已经链接了[`prof_cpu_handler`](../rpc/protocol/http/builtin/prof_cpu_handler.h)。运行中可以`Http`访问`uri`: `/prof/cpu/start`, `/prof/cpu/view`, `/prof/cpu/stop`分别来开启, 查看, 停止。需要在运行机器上提前安装`pprof`, 在`/prof/cpu/view`的时候会通过`shell`调用`pprof`分析采样结果生成对应的函数调用图。采样频率由环境变量`CPUPROFILE_FREQUENCY`控制, 默认每秒钟100次。

## Mem Profiler

提供了[`TcmallocProfilerHttpHandler`](../rpc/builtin/tcmalloc_profiler_http_handler.h)和[`JemallocProfilerHttpHandler`](../rpc/builtin/jemalloc_profiler_http_handler.h)分别适用于使用`tcmalloc`或`jemalloc`来分配内存的程序。
两者只能选其一链接, 同时链接编译时就会报错。链接方式为在`bin`程序`BUILD`中依赖`//flare/rpc/builtin:tcmalloc_profiler_http_handler`或`//flare/rpc/builtin:jemalloc_profiler_http_handler`, 可以参考[例子](../example/http/BUILD)中的`cc_binary`:`server`以及`je_server`。

### Tcmalloc Mem Profiler

需要在运行机器上提前安装`pprof`。

当链接`tc_prof_mem_handler`后, 程序启动就已经开始采样了, 可以通过`Http`访问`uri` : `/prof/mem/view`来查看采样的结果调用图。

可以启动时设置环境变量`TCMALLOC_SAMPLE_PARAMETER`来控制采样频繁, 当程序每分配此大小的内存时就会进行一次采样, 默认值524288(512K)字节为官方建议设置值, 设置小了对程序的性能影响较为明显。

### Jemalloc Mem Profiler

需要在运行机器上提前安装`jeprof`。

当链接`je_prof_mem_handler`后, 如果想要在程序运行期间开启采样功能, 需要在程序启动时设置环境变量`MALLOC_CONF`为`prof:true`来开启采样功能。设置后采样就会默认开启。如果想启动时先不激活, 可以设置`MALLOC_CONF`为`prof:true,prof_active:false`。然后访问`uri`: `/prof/mem/start`, `/prof/mem/view`, `/prof/mem/stop` 来动态开启, 查看, 停止。

可以启动时设置环境变量`MALLOC_CONF`中的`lg_prof_sample`来控制采样频率, 默认频率也为每分配512K字节采样一次。

---
[返回目录](README.md)
