# 压力测试

可以使用[`wrk`](https://github.com/wg/wrk)或编译后的`press`程序进行。

## wrk

服务端运行命令：`./server --logtostderr --flare_concurrency_hint=40`

为使用`wrk`进行压测，首先需要保存如下内容为`echo.lua`:

```lua
wrk.method = "POST"
wrk.body   = "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
```

后执行`./wrk -t24 -c200 -d60s http://127.0.0.1:8888/path/to/echo.svc -s echo.lua --latency`即可。

（76C CVM结果，YMMV）

```text
Running 1m test @ http://127.0.0.1:8888/path/to/echo.svc
  24 threads and 200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    81.55us  118.92us  15.16ms   99.77%
    Req/Sec    92.04k     6.66k  127.14k    85.76%
  Latency Distribution
     50%   79.00us
     75%   85.00us
     90%   91.00us
     99%  110.00us
  132054722 requests in 1.00m, 20.17GB read
Requests/sec: 2197275.87
Transfer/sec:    343.66MB
```

## press

服务端运行命令：`./server --logtostderr --flare_numa_aware --flare_concurrency_hint=32 --flare_scheduling_group_size=8`

客户端运行 `./press --url=http://127.0.0.1:8888/path/to/echo.svc --logtostderr --flare_numa_aware --flare_concurrency_hint=32 --flare_scheduling_group_size=8 --max_pending=256`

客户端主要可以通过设置`flag`:`flare_http_engine_workers_per_scheduling_group`来提升吞吐量。

（76C CVM结果，YMMV）

QPS：

|每个调度组的workers数量         |qps   |延迟avg   |延迟p99   | cpu(客户端) %
|---------------|-----|-----|-----|-----|
|1         |140k  |347us |585us | 620
|2         |260K  |466us  |1351us  | 1187
|4         |450K  |430us |1677us |2200 |
|8         |650K  |301us |1859us |3900 |
