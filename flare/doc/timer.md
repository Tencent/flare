# 定时器

flare 提供了两种风格的定时器：**Fiber 定时器**（业务用、回调跑在 fiber 上）和 **TimeKeeper**（框架内部用、回调跑在专用线程上）。两者背后共享同一套时间堆/排程机制，但服务对象不同。

## 选哪个

| 需求 | 用哪个 |
|---|---|
| 业务代码里 "5 秒后做一件事" / "每 100ms 执行一次健康检查" | [`flare::fiber::SetTimer`](../fiber/timer.h) |
| 在 fiber 里阻塞等待一段时间 | [`flare::fiber::SleepFor` / `SleepUntil`](../fiber/this_fiber.h)（内部走 `WaitableTimer`） |
| `flare/base/` 等底层组件需要周期性 housekeeping | [`flare::internal::TimeKeeper`](../base/internal/time_keeper.h) |
| 实现 RPC 超时、连接保活等框架内部行为 | TimeKeeper 或 fiber 定时器，视调度组上下文而定 |

一句话：**业务和 fiber 上下文里能用的代码用 fiber timer；framework 底层（不能假设有 fiber runtime）用 TimeKeeper。**

## Fiber 定时器

[`flare::fiber::SetTimer`](../fiber/timer.h) 是面向业务的 API：

```cpp
namespace flare::fiber {

// 单次定时
[[nodiscard]] std::uint64_t SetTimer(
    std::chrono::steady_clock::time_point at,
    Function<void()>&& cb);

// 周期定时
[[nodiscard]] std::uint64_t SetTimer(
    std::chrono::steady_clock::time_point at,
    std::chrono::nanoseconds interval,
    Function<void()>&& cb);

// 周期定时（首次 `now + interval` 触发）
[[nodiscard]] std::uint64_t SetTimer(
    std::chrono::nanoseconds interval,
    Function<void()>&& cb);

void KillTimer(std::uint64_t timer_id);
void DetachTimer(std::uint64_t timer_id);
void SetDetachedTimer(...);                          // = DetachTimer(SetTimer(...))

}  // namespace flare::fiber
```

### 关键约定

- **回调跑在 fiber 上**。回调里可以 `this_fiber::Yield()`、可以等其它 fiber、可以发 RPC。不会阻塞 OS 线程。
- **`[[nodiscard]]` ID**：必须 `KillTimer` 或 `DetachTimer`，否则 abort。这是显式的"防忘记"——直接丢弃 ID 是 bug。
- **只能在 scheduling group 内调用**。也就是说必须从已经在 fiber runtime 里跑的代码里发起；从一个裸 pthread 直接 `SetTimer` 会失败。
- **`DetachTimer`** 不取消定时器，只是放弃所有权——周期定时器自我管理生命周期的常见模式。

### 示例

```cpp
// 一次性
auto timer_id = flare::fiber::SetTimer(
    flare::ReadSteadyClock() + 5s,
    [] { FLARE_LOG_INFO("5 秒到了"); });
// 用完必须显式销毁：
flare::fiber::KillTimer(timer_id);

// 周期性（每 100ms 健康检查），不打算手动停：
flare::fiber::SetDetachedTimer(100ms, [] { CheckHealth(); });
```

### 阻塞等待

业务里更常见的需求是"在 fiber 里等一段时间再继续"——这种用 `this_fiber::SleepFor` 而不是 SetTimer：

```cpp
flare::this_fiber::SleepFor(100ms);   // 当前 fiber 挂起 100ms
// 100ms 后从这里继续
```

`SleepFor` 内部用 [`WaitableTimer`](../fiber/detail/waitable.h)：定时到期时唤醒当前 fiber，期间 worker 线程跑别的 fiber。

## TimeKeeper

[`flare::internal::TimeKeeper`](../base/internal/time_keeper.h) 是 framework 底层使用的定时器。

存在的理由：`flare/base/` 的若干组件（如 object pool 的周期 GC、监控指标的周期采样）需要定时任务，但它们位于 fiber runtime 的**下层**——这些组件初始化时 fiber runtime 还不一定就绪，且这些回调本身不需要 fiber 上下文。专门给它们做一个轻量级共享定时器。

```cpp
namespace flare::internal {

class TimeKeeper {
 public:
  // 全局单例
  static TimeKeeper* Instance();

  std::uint64_t AddTimer(
      std::chrono::steady_clock::time_point at,
      std::chrono::nanoseconds interval,
      Function<void(std::uint64_t)>&& cb,
      bool dispatch_in_fiber = false);

  void KillTimer(std::uint64_t timer_id);
};

}  // namespace flare::internal
```

### TimeKeeper vs Fiber timer

- **回调线程**：TimeKeeper 默认在专用 OS 线程上跑（除非 `dispatch_in_fiber=true`），fiber timer 总是跑在 fiber 上
- **谁用**：TimeKeeper 给 `flare/base/` 自己用，**业务代码不该直接用**——头文件注释专门说了一句"For other components, or user code, consider using fiber timer instead"
- **生命周期**：TimeKeeper 是全局单例，进程级；fiber timer 跟随 scheduling group

## 实现机制

底层有两套排程：

### TimerWorker（fiber timer 背后）

[`flare/fiber/detail/timer_worker.h`](../fiber/detail/timer_worker.h)

- 每个 scheduling group 一个 `TimerWorker`，独立 OS 线程
- 维护一个**按到期时间排序的小顶堆**
- 主循环：取堆顶 → `wait_until` 睡到到期点 → 触发回调（投回 scheduling group 让 fiber 跑）
- 新增的定时器如果到期早于堆顶，通过 condition_variable 唤醒主循环重新排程
- 周期定时器在触发后自己重排（`expires_at += interval`）

### TimeKeeper 内部

[`flare/base/internal/time_keeper.cc`](../base/internal/time_keeper.cc)

结构同上（堆 + 专用线程），更简化：没有 fiber 分发逻辑，回调直接在 keeper 线程上调用。

### 精度

- 调度精度取决于 OS 的 `futex` / condition_variable 的唤醒精度，典型情况下 **<1 ms**
- 不适合做亚毫秒级精确定时（实时系统场景）；那种需求需要 OS 层 timer / spin

## 取消语义

`KillTimer` 不保证"取消即不会触发"——存在如下竞态：

1. 定时器到期，回调已被投递到 scheduling group queue
2. `KillTimer` 调用
3. 回调还是会被执行（已 inflight）

这是 fire-and-forget 模式的代价。需要严格"取消生效"的业务，应该在回调里自己再判断一次状态（例如检查某个 atomic flag 是否仍为 active）。

## 与其它子系统的关系

- [Fiber 调度](fiber-scheduling.md)：定时器到期产生 ready fiber，纳入正常调度
- [WaitableTimer](../fiber/detail/waitable.h)：fiber-style "在某个时间点醒来"原语，`SleepFor` 的底层
- [Monitoring](monitoring.md)：用 TimeKeeper 周期 flush 指标
- [Object Pool](object-pool.md)：用 TimeKeeper 周期 GC 空闲对象

---
[返回目录](README.md)
