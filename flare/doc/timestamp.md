# 时间戳

flare中大量存在各种时间戳，因此如何获取时间戳也自然是首要问题之一。

通常我们可以直接通过[`std::chrono::steady_clock::now()`](https://en.cppreference.com/w/cpp/chrono/steady_clock/now)来读取时间戳，但是对于频繁获取时间戳的情况，这儿存在的性能问题就不得不仔细进行分析。

这篇文档主要解释了flare中使用到的或可能会涉及到的时间获取方式。

## `std::chrono::xxx_clock::now()`

`std::chrono::xxx_clock::now()`是最常见的获取系统时间的方式，但是取决于环境，**[这可能会引入syscall而影响性能](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=59177)**。

对于CentOS6 / tlinux1.2环境下编译的GCC，如果没有在`configure`阶段指明[`--enable-libstdcxx-time=rt`](https://gcc.gnu.org/onlinedocs/libstdc++/manual/configure.html)（而非单纯的指定`--enable-libstdcxx-time`），那么为了避免引入对`rt`的依赖，libstdc++会使用`syscall`实现`std::chrono::xxx_clock::now()`。

而目前公司内流传的自行编译的GCC多有这一问题。

为了保证在各编译环境下尽可能的一致，flare提供了使用[vDSO](https://en.wikipedia.org/wiki/VDSO)实现的[相应方法](../base/chrono.h)（`ReadXxxClock()`）。

## `ReadXxxClock()`

如上所述，我们提供了自己使用vDSO实现的获取时间戳的方法：

- `ReadSteadyClock()`：对应`std::chrono::steady_clock::now()`，二者获取的时间戳属于同一时钟源，可互换。取决于编译环境，`ReadSteadyClock()`的性能可能好于或等于（但不会劣于）`std::chrono::steady_clock::now()`。
- `ReadSystemClock()`：对应[`std::chrono::system_clock::now()`](https://en.cppreference.com/w/cpp/chrono/system_clock/now)，同一时钟源，可互换。性能不低于`std::chrono::system_clock::now()`。

尽管如此，`ReadXxxClock()`的性能通常仍会低于通过[`rdtsc`](https://www.felixcloutier.com/x86/rdtsc)实现的[`ReadTsc()`](../base/tsc.h)。

这是因为，通常而言，系统时钟是在内核共享的时间戳上加上增量（取决于[系统的时钟源](https://access.redhat.com/solutions/18627)，对于物理机通常是`tsc`，虚拟机通常是通过hypervisor提供的，如`kvm`）获取的，因此其计算成本包含且不限于`rdtsc`，相对于直接读取`tsc`会更慢。

## `ReadTsc()`

但是需要注意的是，**使用`tsc`作为时钟通常不是一个好的选择**：

- [`tsc`可能会受到CPU的节能状态的影响](https://patchwork.kernel.org/patch/4043361/)
- 虚拟化环境下虚拟机之间迁移可能导致`tsc`漂移或速率变化（[Intel VT-x](https://www.intel.com/content/dam/www/public/us/en/documents/white-papers/timestamp-counter-scaling-virtualization-white-paper.pdf)及[AMD-V](https://lore.kernel.org/patchwork/patch/235892/)提供了相应的补偿措施，具体取决于hypervisor实现）
- `tsc`和物理时钟之间转换可能需要[CPI](https://en.wikipedia.org/wiki/Cycles_per_instruction)较大的除法操作（但是参考下文的`XxxFromTsc`）

因此我们通常只推荐将`tsc`用于忙等或其他特殊场景，而不用于通用计时。仅当清楚自己在做什么的前提下，才可以使用`tsc`来计时。

*目前我们在频繁获取时钟并且对精度要求较高（如记录RPC过程中识别消息边界、解包、打包等等）的场景使用TSC作为时间戳，这是一个内部实现细节并且随时可能发生变动。这些场景下，在将时间戳最终提供给用户之前，我们会将其转换为物理时间（参考下文）。*

另外需要注意的是，尽管`rdtsc`只有一条指令，这条指令的CPI并不低。根据[Agner的数据](https://www.agner.org/optimize/instruction_tables.pdf)，`rdtsc`在[Skylake](https://en.wikipedia.org/wiki/Skylake_(microarchitecture))上需要25个时钟周期，2.5GHz主频（我们的服务器环境通常在2GHz ~ 3GHz主频）下需要10ns。

同时，对于性能要求高但可以容忍一定误差（如对象池计算对象生存时长）的场景，提供了性能比`ReadTsc()`更高的[`ReadCoarseXxxClock()`](../base/chrono.h)。

**再次强调，正确的使用`tsc`非常困难。用于时间戳目的时，始终应当优先考虑其他可选项。**

### TSC转换为物理时间

我们提供了`DurationFromTsc(...)`及`TimestampFromTsc(...)`用于将`tsc`区间/`tsc`值转换为物理时间。这两个方法我们做了一定的优化，在我们的[测试](../base/tsc_benchmark.cc)中，开销并不明显（和benchmark框架空转的测试结果都是2ns）。

由于实现限制，**对于较大的时间差（如数千秒），`DurationFromTsc`内部可能会出现整型溢出导致不正确的结果**。通常我们只建议将`DurationFromTsc`用于计算较短且精度要求高的耗时（如单次RPC开销等）。

在考虑通过`tsc`维护时间戳之前，我们需要再次明确，**使用`tsc`作为时钟通常不是一个好的选择。**。

#### `DurationFromTsc`

内部而言，我们会假定`tsc`速率是恒定的，并在在程序启动时（通过空循环）计算256M个时钟周期所对应的物理时间，以之来计算`tsc`和物理时间的比率。

*我们选择256M个时钟周期是因为`物理时间 = tsc * 256M个时钟周期对应的物理时间 / 256M`。在这种情况下，除以256M可以被优化为移位操作，改善性能。对于2GHz主频的机器而言，256M大约100ms。*

#### `TimestampFromTsc`

内部而言我们给每个线程都通过一个线程局部变量保存了一个未来时间点的物理时间及其对应的`tsc`值。在计算时我们首先算出来当前`tsc`距离这个未来的`tsc`的时间差（通过`DurationFromTsc`），然后从未来的物理时间点减去这个时间差得到当前的物理时间。

*代码中我们做了一定的设计，避免对TLS的动态初始化（即避免调用`__tls_init`），具体可以参考[代码](../base/tsc.h)。*

我们之所以选择未来的时间点而不是过去的时间点主要是考虑到我们计算的`tsc`和物理时间的比率并不精确（`tsc`*精度*高不代表*精确*）。

通过使用未来的时间戳，我们可以定期的发现时间戳过期并重新计算，使得误差始终保持在一个合适的范围之内。

同时，这也会减轻NUMA节点之前迁移导致的`tsc`漂移问题：当目前的“未来时间戳”过期之后，这个线程会在新的NUMA节点重新计算这个值，因此迁移导致的`tsc`漂移对时间转换的影响是临时性的。只要线程不会在NUMA节点之间频繁的迁移（一个合理的调度算法通常都会避免这种行为），就不会对我们造成太大的影响。

## `ReadCoarseXxxClock()`

内部实现而言，`ReadCoarseXxxClock()`通过旁路定期更新来加速读取。这一行为类似于[`CLOCK_MONOTONIC_COARSE`](https://linux.die.net/man/2/clock_gettime)（通过时钟中断更新）。

*确切来说，我们曾经的实现确实通过`CLOCK_MONOTONIC_COARSE`实现，但是后续进行了优化，见下文。*

但是`clock_gettime(CLOCK_MONOTONIC_COARSE)`实际上会有不必要的函数调用开销（并且不低，但略低于`ReadTsc()`）。受trpc-cpp性能优化（3.5节）的启发，我们自行实现了类似的逻辑。因此`ReadCoarseXxxClock()`会被内联并编译为直接读取内存。

由于共享的时钟信息定期更新，因此**可能存在一定量的延迟（即误差）**。我们的实现通常可以提供不超过10ms的误差（程序极端繁忙导致异步更新延迟可能会导致时间戳误差增大）。

但是由于其内部无“复杂”的操作，因此其实际性能表现优异，是本文提及的时间获取方式中最快的。

## 实际性能

由于某些方法（主要是`std::chrono::xxx_clock::now()`及`ReadXxxClock()`）在不同环境下性能差异过大（是否触发`syscall`、时钟源是`tsc`、`kvm`、...，Xeon Scalable / EPYC，系统内核版本），因此此处不提供性能数据。

但是我们提供了针对不同方法的测试代码：

- [`std::chrono::xxx_clock::now()`、`ReadXxxClock()`、`ReadCoarseXxxClock()`](../base/chrono_benchmark.cc)
- [`ReadTsc()`](../base/tsc_benchmark.cc)

如有需要可自行测试。

---
[返回目录](README.md)
