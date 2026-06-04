# 时间戳

flare 中大量需要读取时间戳——RPC 流程中的解包/打包计时、定时器排程、监控指标采样、对象池 GC 等都依赖它。读时间戳本身看起来"应该 1 个指令的事"，实际上在不同实现下差异巨大。

这篇文档介绍 flare 中可用的时间获取方式、性能特征和选用原则。

## 简明选用指南

| 场景 | 用哪个 |
|---|---|
| 默认 / 业务代码 / 不在意纳秒级别开销 | `ReadSteadyClock()` / `ReadSystemClock()` |
| 高频读取但允许 ~10ms 误差（对象池、监控采样、限流时间窗等） | `ReadCoarseSteadyClock()` / `ReadCoarseSystemClock()` |
| 框架内部需要纳秒精度、且能容忍 TSC 的脆弱性（RPC 解包阶段计时） | `ReadTsc()` + `DurationFromTsc()` / `TimestampFromTsc()` |
| 用户接口、跨进程比较、需要可读时间 | 优先 `ReadSystemClock()`；TSC 时间戳暴露给用户前先转换 |

一句话：**默认用 `ReadSteadyClock`；不够快换 `ReadCoarse*`；只有完全清楚 TSC 风险时才用 TSC。**

## `ReadSteadyClock()` / `ReadSystemClock()`

[`flare/base/chrono.h`](../base/chrono.h)

```cpp
inline std::chrono::steady_clock::time_point ReadSteadyClock();
inline std::chrono::system_clock::time_point ReadSystemClock();
```

**当前实现就是直接 `std::chrono::steady_clock::now()` / `system_clock::now()`** —— 不做任何额外封装。

历史上这两个函数曾经绕过 libstdc++、自己用 `clock_gettime` 直读 vDSO，是为了规避 CentOS 6 / 老 tlinux 上自编译 GCC 的 `_GLIBCXX_USE_CLOCK_GETTIME_SYSCALL` 走真 syscall 的性能坑。**这个坑在现代工具链上不存在**——任何 GCC 9+ / Clang 10+ 都通过 vDSO 直接进 `__vdso_clock_gettime`。

更重要的是，自己绕过 `std::chrono` 直读 `clock_gettime` 会引入**时钟纪元不一致**的隐患：在 macOS（Darwin 24 / macOS 15）上，`CLOCK_MONOTONIC` 和 `std::chrono::steady_clock::now()` 的纪元（epoch）可以不同——前者会累计 sleep/suspend 时长，后者不会。混用两者构造出来的 `time_point` 会无声地偏移，曾经导致 `TimerWorker::cv_.wait_until()` 时间错乱。所以现在统一走标准库。

### 性能

跟 `std::chrono::steady_clock::now()` **完全等同**。在 Linux x86_64 vDSO 下大约 **20-30 ns**；在 ARM / 不同时钟源（kvm-clock、hpet 等）下浮动较大，但都在 100 ns 量级以内。

## `ReadCoarseSteadyClock()` / `ReadCoarseSystemClock()`

[`flare/base/chrono.h`](../base/chrono.h)

```cpp
inline std::chrono::steady_clock::time_point ReadCoarseSteadyClock();
inline std::chrono::system_clock::time_point ReadCoarseSystemClock();
```

**精度 ~10ms 的"快粗"时钟。**

实现：进程启动时启一个后台线程（`CoarseClockInitializer::worker_`），周期性地把 `steady_clock::now()` / `system_clock::now()` 写到全局原子变量里；读侧只是一次 `atomic::load`——**会被内联编译为直读内存**。

这跟 Linux 的 `clock_gettime(CLOCK_MONOTONIC_COARSE)` 思路一致（通过时钟中断更新缓存），但避开了 `clock_gettime` 的函数调用开销。设计灵感来自 tRPC 性能优化文档第 3.5 节，flare 自己实现了这个 trick。

### 误差

后台线程**正常工作**时，误差 < 10ms。极端 CPU 繁忙（更新线程被饿到）时误差可能更大——所以**不要用它做精确计时**（如 RPC 超时判断），只用它做**对精度不敏感的高频读取**（对象池年龄判断、监控时间窗等）。

### Coarse 的性能

是本文所有方式中**最快**的——单次读取就是一次缓存命中的 atomic load，几乎 0 开销，比 `ReadTsc()` 还快（TSC 还有 `rdtsc` 的微码 cost）。

## `ReadTsc()`

[`flare/base/tsc.h`](../base/tsc.h)

```cpp
inline std::uint64_t ReadTsc();
```

直接发 [`rdtsc`](https://www.felixcloutier.com/x86/rdtsc) / 对应架构的等价指令读时钟计数器。

### TSC 的几个坑

**用 TSC 当通用时钟通常是个坏主意**：

- TSC 可能受 [CPU 节能状态影响](https://patchwork.kernel.org/patch/4043361/)（虽然现代 x86 普遍 "invariant TSC"——constant rate 不受 P-state 影响——已是事实标配，但不保证 100%）
- 虚拟化环境下虚拟机迁移可能引起 TSC 漂移或速率变化（[Intel VT-x](https://www.intel.com/content/dam/www/public/us/en/documents/white-papers/timestamp-counter-scaling-virtualization-white-paper.pdf) / [AMD-V](https://lore.kernel.org/patchwork/patch/235892/) 提供补偿，但**取决于 hypervisor 实现**）
- TSC ↔ 物理时间换算需要除法（CPI 高），见下文 `DurationFromTsc` 的优化

而且 `rdtsc` 本身 CPI 并不低——根据 [Agner](https://www.agner.org/optimize/instruction_tables.pdf) 的数据，Skylake 上 ~25 cycle，2.5 GHz 下约 **10 ns**。比 `ReadCoarseSteadyClock` 慢。

**flare 内部哪儿用了 TSC**：

当前在 RPC 解包阶段记录消息边界、序列化/反序列化耗时等高频高精度场景使用了 TSC（[example](../base/tsc_benchmark.cc) 里有测试代码）。这是**内部实现细节**，随时可能改。**对外暴露给用户的时间戳一定先 `TimestampFromTsc` 转换成物理时间**。

### TSC → 物理时间：`DurationFromTsc` / `TimestampFromTsc`

`DurationFromTsc(tsc_a, tsc_b)` 返回 `b - a` 的物理时长，`TimestampFromTsc(tsc)` 把 TSC 值转成 `steady_clock::time_point`。

[`tsc_benchmark.cc`](../base/tsc_benchmark.cc) 测试下两者开销都接近 benchmark 框架空转（2 ns），说明优化生效了。

#### 优化原理：`DurationFromTsc`

程序启动时（通过空循环）测算 **256M 个 TSC tick 对应的物理时长**，记为 `T₂₅₆ₘ`。换算公式：

```text
物理时长 = (tsc 差) × T₂₅₆ₘ / 256M
```

除以 256M 可以被编译器优化为右移 28 位——避免了昂贵的整数除法。256M 在 2 GHz 主机上大约 100 ms，长度合理。

#### 优化原理：`TimestampFromTsc`

每个线程通过线程局部变量缓存"**未来某时刻**"的物理时间 + TSC 值。读时算"当前 TSC 距离这个未来 TSC 的差"（用 `DurationFromTsc`），再从未来物理时间反推当前。

为啥取未来不取过去：

1. TSC ↔ 物理时间比率不绝对精确（精度高 ≠ 精确）。用未来时间戳意味着会定期"到期"重新校准，误差不会无界累积。
2. 缓解 NUMA 迁移导致的 TSC 漂移——线程被调度到新 NUMA 节点后，下次"到期"时会用新节点的 TSC 重新校准，影响仅一次。

代码里做了一定的设计避免 TLS 动态初始化（`__tls_init`），详见 [tsc.h](../base/tsc.h)。

#### `DurationFromTsc` 的限制

**`DurationFromTsc` 对超长时间段（几千秒）可能内部溢出导致结果错误。** 只适合短时高精度计时（如单次 RPC 全程耗时），别拿它算"程序启动到现在多久了"。

## 性能对比（数量级）

实际数字因 CPU / 时钟源 / 系统版本浮动大，**不给具体值**；只给数量级排序，从最快到最慢：

```text
ReadCoarseXxxClock     ~ 1-2 ns    （内存读，最快）
ReadTsc                ~ 10 ns     （rdtsc 微码）
DurationFromTsc        ~ 2 ns      （位移 + 乘法）
TimestampFromTsc       ~ 2 ns      （+ TLS 读 + 偶发重校准）
ReadSteadyClock        ~ 20-30 ns  （vDSO clock_gettime；视时钟源浮动）
ReadSystemClock        ~ 20-30 ns  （同上）
```

要自己测的话：

- [`chrono_benchmark.cc`](../base/chrono_benchmark.cc)：`std::chrono::*::now()` / `ReadXxxClock` / `ReadCoarseXxxClock`
- [`tsc_benchmark.cc`](../base/tsc_benchmark.cc)：`ReadTsc` / `Duration|TimestampFromTsc`

## 决策树

```text
需要时间戳
├── 高频 + 容忍 ~10ms 误差?
│   └── 是 → ReadCoarseXxxClock         (1-2 ns, 内存读)
├── 高频 + 纳秒精度 + 框架内部 + 清楚 TSC 风险?
│   └── 是 → ReadTsc + DurationFromTsc  (12 ns 共, 但要懂限制)
└── 否（默认）
    └── ReadSteadyClock / ReadSystemClock   (20-30 ns, 标准实现)
```

---
[返回目录](README.md)
