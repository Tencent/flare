# 监控

Flare对于程序内部上报统计提供了[统一的支持](../base/monitoring.h)。

通常来说，可行的情况下，使用Flare的二次包装有包括但不限于如下优点：

- 易于替换/支持新的监控系统。

  Flare对监控系统进行了抽象，[使用相同的接口](../base/monitoring/monitoring_system.h)来接入多种监控系统。通常而言业务可以通过参数`--flare_monitoring_system=`来指定其希望使用的监控系统（如果使用多个，通过`,`分隔）。

  目前我们支持下列监控系统：

  - ...

  通常而言接入新的监控系统不涉及到业务代码的修改，只需要修改配置并实现对应的[`flare::MonitoringSystem`](../base/monitoring/monitoring_system.h)接口即可。

- 更好的性能。

  根据我们的经验，大多数的监控系统的SDK并非**高度**优化的。这体现在包括但不限于如下编码方式：

  - 同步通过UDP上报至同机的agent：这可能会在业务路径中引入不必要的`syscall`，增大业务延迟。
  - 使用全局无锁队列或其他无锁结构传递上报数据：这种实现通常每次上报均会操作相应的无锁结构。这在多线程/多核（数十或更多）环境下通常意味着CPU内部高度竞争，上报数据量大时有明显的性能开销。更多相关讨论可以参考[我们的性能指引](performance-guide.md)。
  - 未经优化的朴素实现。

  考虑到业务代码通常在关键路径中同步执行上报逻辑，监控系统的SDK的任何不够优的实现都会直接对业务响应造成影响，因此我们不推荐业务直接使用监控系统的SDK。

  Flare中，我们做了一些（见后文）优化保证上报性能尽可能地不影响业务处理的关键路径。另外，由于我们所做的优化均发生在框架层面，**无论使用何种监控系统（只要其SDK实现性能相对合理），业务均可以得到优异的最终性能表现**。

综上，我们建议业务统一使用[Flare的监控上报能力](../base/monitoring.h)。

## 对各个特性定义变量方式上报

这种上报方式中Flare框架会在将数据交给SDK之前，自行进行数据合并。

这通常有更好的性能，且对编码、阅读更加友好。在可能的情况下，这是我们推荐的使用方式。

这种方式下，程序应当将需要上报的特性定义为全局（或命名空间级）变量，之后通过对应的变量上报，如：

```cpp
MonitoredCounter processed_req(
    "flare_attr_name_for_reqs", {{"some-costless-tag", "and its value"}});

void FancyServiceMethod() {
  processed_req.Increment();  // Report a new processed request.
}
```

部分监控系统上报时支持额外携带Tags，我们同样对这一能力提供了支持，具体可以参考文件内注释。需要注意的是，单次上报时额外携带Tag会有少量通常不可感知的性能损耗（构造时传入Tag不会影响性能）。

### `MonitoredTimer`

特别的，尽管我们期望各监控系统的SDK都能原生的提供对延时的支持，但是实际上绝大多数监控系统都通过普通的“求平均”来实现监控延时，因此最终上报给SDK的数据应当是一个整数而不是一个[`std::duration`](https://en.cppreference.com/w/cpp/chrono/duration)。

对于这种情况，我们允许定义`MonitoredTimer`时传入时间单位（通过参数`unit`），实际上报之前Flare会负责进行单位转换。

默认情况下Flare以微秒为单位上报，如希望以毫秒为单位上报，可以使用如下方式：

```cpp
MonitoredTimer process_latency("some_processing_latency", 1ms);

void BoringMethod() {
  auto start = ReadSteadyClock();
  // ... Do something.
  process_latency.Report(ReadSteadyClock() - start);
}
```

## 直接上报单个数据

这种上报方式下Flare框架不会进行对数据进行合并，每次上报最终均原样上报给SDK。

在`MonitoredXxx`不能满足需求或不被所使用的监控系统支持时，可以使用这种方式。

部分监控系统支持额外携带Tag，我们同样对这一能力提供了支持，但是可能会有少量通常不可感知的性能损耗。

受限于具体的监控系统，上报时可能需要提供期望的数据合并方式（`expected_reading`参数）：

```cpp
void AnotherMethod() {
  flare::monitoring::Report(
      flare::monitoring::Reading::Sum, "some_accumulating_key", 10,
      {{"tag-key", "value"}});
}
```

如果已知将要使用的监控系统不要求提供数据合并参数，则可以使用不需要`expected_reading`的重载版本。

*但是这可能会限制后期通过修改`flare_monitoring_system`替换监控系统的能力。*

## 同时使用多个监控系统

可以通过配置项（见后文）`--flare_monitoring_system=xxx,yyy,zzz,...`的方式同时使用多个监控系统，框架会负责将事件依次上报至各个监控系统。

*因为我们的上报是后台线程异步执行的，因此只要监控系统的SDK的实现能够提供基本合理的性能，上报至多个监控系统不会影响正常的请求处理速度。*

某些情况下可能上报至多个监控系统时，不同的监控系统需要使用不同的key（如同时上报至观星台和tnm时，上报至tnm需要使用数字ID）。对此我们提供了参数`--flare_monitoring_key_remap`（见后文）来配置如何在上报至具体监控系统之前对key进行映射。

## Flare内置监控

为了方便用户更为全面的了解服务内部状态，Flare内部也会进行一定的监控数据上报。

考虑到不同业务通常使用不同的上报Key，因此Flare默认情况下相关的上报是关闭的。这儿需要通过配置指定一个或多个用户关注的特性及需要上报到的Key的名称（参考后文配置项`flare_monitoring_builtin_key_mapping`），才会进行上报。

目前Flare内置了如下上报项：

- [`flare_fiber_latency_ready_to_run`](../fiber/detail/scheduling_group.cc)：Fiber从就绪状态到开始执行之间的调度延迟，单位微秒。

## 配置

为了使用监控系统，框架级别需要配置如下参数：

- `flare_monitoring_system`：实际使用的监控系统。通常一个程序应当只使用一个监控系统，但是如果在监控系统的迁移过程中则可能需要使用多个监控系统，那么可以通过`,`分隔并分别列出。如：`--flare_monitoring_system=gxt`

另外框架级别有如下可选参数：

- `flare_monitoring_key_remap`：如果不为空，这个参数指定了代码中的key和实际上报的key的映射关系。

  比如可以通过这一配置，将代码中的`MonitoredCounter counter1("some-fancy-counter")`实际上报（如上报至tnm）时的key替换为`123456`。

  这一参数应当是一个文件路径，或`sys1=path/to/sys1_remap.yaml,sys2=path/to/sys2_remap.yaml,...`格式的字符串。

  - 如果配置为文件路径，则所有的上报都会进行一次映射。
  - 第二种格式（`sys=path,...`的格式）通常只在同时上报至多个监控系统时使用，这样的配置允许针对不同的监控系统进行不同的映射关系。

    这种格式中如果某个监控系统没有指定映射关系，则直接使用代码中的key上报。

  具体的文件内容使用[yaml](https://yaml.org/)格式，配置项及其含义如下：

  ```yaml
  passthrough-on-missing: yes  # If set, keys not found in `keys` section are
                               # reported as-is. Otherwise they're silently
                               # ignored (possibly leave a warning.).
  keys:
    some-fancy-counter: 123456
    another-fancy-counter: 2345678
    # Keys not listed here are reported as-is (since `passthrough-on-missing` is
    # set).
  ```

  尽管二次映射也是在后台线程进行，但是考虑到进行二次映射依然会有一定的性能成本，同时通常一个程序只使用一个监控系统，因此**通常我们建议代码中直接使用实际上报的key**（即将这个参数留空）。

- `flare_monitoring_builtin_key_mapping`：如果不为空，这个参数指定了Flare内置监控项的上报Key的映射关系。

  *如果这个参数未配置，则Flare自身的所有监控项均不会上报。*

  这一参数应当是一个YAML配置文件的路径。文件的配置格式如下：

  ```yaml
  keys:
    flare_fiber_latency_ready_to_run: key_to_report_to_monitoring_sys_1
    # ...
  ```

  配置文件中在`keys`下面有一系列KV对，键名可选项均在“Flare内置监控”列出，键值取决于用户使用的监控系统。

  内部而言，上报的Key在经过这个文件映射之后，依然会再经过`flare_monitoring_key_remap`映射一次。这允许用户将相同的内部特性通过不同的Key上报至不同的监控平台。

- `flare_monitoring_extra_tags`：如果不为空，这一参数可以通过`K1=V1;K2=V2`指定一系列KV对作为Tag。这些Tag将会附加在*所有*的`MonitoredXxx`上。如果相同的Key同时出现在了`MonitoredXxx`的构造函数或上报方法的`tags`参数，则这些更细粒度的参数优先级更高（覆盖全局设置）。

针对不同的监控系统，可能还需要配置不同的参数，下面列出了各个监控系统特有的参数。

## 内部优化

内部而言，我们做了如下一些优化尽量保证监控上报耗时稳定、耗时低且具备出色的多核伸缩性：

- 在关键路径上不调用监控系统SDK。这最大程度的避免了监控SDK的实现质量对服务整体性能造成影响。

- 对于[`flare::MonitoredXxx`](../base/monitoring.h)：绝大多数情况下只操作线程局部缓存的上报数据，每个线程每秒只会进行至多一次数据整合并尝试上报。

  这儿检查时间用的是[粗略的时间戳，x86上通常编译为普通的内存读取](timestamp.md)。

  尝试上报时不会同步调用监控系统SDK，进行基本的现场保存之后即[交由后台线程](../base/internal/dpc.h)（我们的实现这儿同样避免了syscall等开销大的操作）异步上报并返回。因此这一“slow path”实际上也执行得非常快。

  [在我们的测试中](../base/monitoring_benchmark.cc)，单次上报通常可以在~2ns完成。

- 特别的，对于`flare::MonitoredTimer`，我们额外优化了对于上报性能特别敏感且上报的时延较低的场景：

  1. 通常来说，对于上报性能敏感的场景，通常意味着自身处理逻辑也较快。

     因此，我们预期会有很多小于100个单位的latency，并**使用数组优化来保存各个latency的个数**。而对于更大的latency，则采用`std::unordered_map`保存节约内存。

  2. 从`std::chrono::duration`转换到期望的时间单位（以判断是否小于100个单位来进行上述优化）涉及到除法操作。

     针对时间单位为`1ns`、`1us`、`1ms`、`1s`的场景，我们单独生成了对应的转换函数（一个无捕获的lambda，内部通过函数指针方法）进行转换。因此将运行时变量（传给`MonitoredTimer`的构造函数的`unit`参数）变成了编译期常量（`1ns` / `1us` / ...），这允许编译器将相应的除法操作通过加法及乘法完成，改善性能。

- 对于`flare::monitoring::Report(...)`：上报操作只会将数据保存在每个线程一个的SPSC无锁有界队列即返回，无额外操作。

  我们使用后台线程定期消耗各个线程的SPSC队列。由于一个队列最多被两个线程使用（生产消费各一个），且竞争频率低（竞争频率=后台线程唤醒频率），因此多核环境下的伸缩性表现出色。

  复制上报的`key`时我们也做了[额外的优化](../base/monitoring/event.h)，考虑到`key`通常较短，我们会优先尝试复制到`flare::monitoring::Event`内部的`char`数组（`CharArray`）中，只有其较长才会实际上复制字符串。

  *尽管我们使用GCC8.2，但是由于我们使用的是pre-GCC5的ABI，因此libstdc++并未实现SSO。另外，pre-GCC5 ABI的`std::string`复制是COW的，这儿COW暗含的原子操作实际上要比少量字节复制更慢。而且取决于`key`的来源，COW可能有伸缩性问题。以后在SSO普遍可用时，我们会改为直接使用`std::string`。*

  另外我们的设计中，如果上报速度超过消费速度（通常意味着系统已经过载了需要降级）会降级服务，丢弃部分上报保证整体稳定性。

  [在我们的测试中](../base/monitoring_benchmark.cc)单次上报通常可以在~6ns完成。

  **通过这种方式上报的数据框架不会进行合并，上报量太大时可能有比较明显的时空性能损耗。**

---
[返回目录](README.md)
