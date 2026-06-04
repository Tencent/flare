# 名字服务和负载均衡

[Channel](channel.md) 在发起 RPC 之前，要先把一个 `flare://example.Service` 之类的服务名解析成一个或多个后端实例地址，再从中选择一个发起调用。这两步分别由**名字解析（Name Resolution）**和**负载均衡（Load Balancing）**完成。

flare 把两者解耦成独立的抽象，便于按场景替换或组合。

## 设计

```text
        ┌───────────────┐
        │   Channel     │
        └───────┬───────┘
                │ name (e.g. "example.Service")
                ▼
   ┌─────────────────────────┐
   │  Path A: NSLB 分离模式   │
   │                         │
   │   NameResolver          │  把 name 解析为 endpoints 列表
   │        │                │
   │        ▼                │
   │   LoadBalancer          │  从列表中按策略挑一个
   └─────────────────────────┘
                          ┌─────────────────────────┐
                          │  Path B: NSLB 整合模式   │
                          │                         │
                          │   MessageDispatcher     │  解析 + 选址一步到位
                          │                         │  （适合 CL5 这类自带选路的服务）
                          └─────────────────────────┘
                                       │
                                       ▼
                                   一个 Endpoint
                                   交给 Channel 发请求
```

两条路径走哪条由 [`MessageDispatcherFactory`](../rpc/message_dispatcher_factory.h) 在构造 Channel 时决定，对调用方透明。

## NameResolver

[`flare::NameResolver`](../rpc/name_resolver/name_resolver.h) 负责把字符串名字翻译成 `Endpoint` 列表。

抽象上分两层：

- **`NameResolver`**：全局单例，按 name 创建一个 `NameResolutionView`。是个轻量工厂。
- **`NameResolutionView`**：单 name 的解析"会话"，提供：
  - `GetPeers(std::vector<Endpoint>*)` —— 拉当前实例列表
  - `GetVersion()` —— 返回当前版本号，外层 LoadBalancer 据此判断是否需要更新

版本号有两个特殊值：

| 值 | 含义 |
|---|---|
| `kNewVersion` | "每次调用都视为新版本" —— 不实现自身缓存的 resolver 用这个 |
| `kUseGenericCache` | "我不缓存，让框架替我缓存" —— 框架会另开线程周期性调 `Resolve()` |

实现既可以是同步阻塞型（DNS-like：一次 RPC 触发一次解析），也可以是异步推送型（订阅注册中心，本地维护快照）。

### NameResolver 内置实现

- [`List`](../rpc/name_resolver/list.h) —— `flare-list://1.2.3.4:80,1.2.3.5:80` 这种 inline 列表，主要用于测试和静态部署
- 业务可通过 `FLARE_RPC_REGISTER_NAME_RESOLVER(scheme, MyResolver)` 注册自己的 scheme（如 `cl5://`、`zookeeper://`、`polaris://`）

### NameResolver 注册机制

```cpp
FLARE_DECLARE_OBJECT_DEPENDENCY_REGISTRY(name_resolver_registry, NameResolver);

// 用户侧：
class MyResolver : public NameResolver { /* ... */ };
FLARE_RPC_REGISTER_NAME_RESOLVER("polaris", MyResolver);
```

注册后通过 URI scheme 自动派发——`Channel("polaris://service.Foo")` 会找到 polaris resolver。

## LoadBalancer

[`flare::LoadBalancer`](../rpc/load_balancer/load_balancer.h) 在 `NameResolver` 给出的 endpoint 列表里按策略选一个。

接口三件套：

```cpp
class LoadBalancer {
  virtual void SetPeers(std::vector<Endpoint> addresses) = 0;
  virtual bool GetPeer(std::uint64_t key, Endpoint* addr,
                       std::uintptr_t* ctx) = 0;
  virtual void Report(const Endpoint& addr, Status status,
                      std::chrono::nanoseconds time_cost,
                      std::uintptr_t ctx) = 0;
};
```

- **`SetPeers`** —— `NameResolutionView::GetPeers` 出新版本时全量替换
- **`GetPeer(key, ...)`** —— 选址；`key` 用于一致性哈希等需要稳定路由的策略
- **`Report`** —— 调用结果反馈（成功/失败/超时/耗时），用于熔断或权重调整

`Status` 当前定义了 `Success` / `Overloaded` / `Failed`（`Overloaded` 暂未启用）。

### LoadBalancer 内置实现

| 实现 | 路径 | 适用场景 |
|---|---|---|
| `RoundRobin` | [round_robin.h](../rpc/load_balancer/round_robin.h) | 默认；轮询；最简单负载均衡 |
| `ConsistentHash` | [consistent_hash.h](../rpc/load_balancer/consistent_hash.h) | 按 `key` 一致性哈希；适合带分片亲和的服务（缓存、按用户 ID 分片的存储） |

### 一致性哈希要点

`ConsistentHash` 通过 `GetPeer(key, ...)` 的 `key` 把同一逻辑实体（如 user_id）路由到同一台后端。调用方需要：

- 在 `RpcClientController` 里设置一致性哈希 key（视协议而定，参见 [Controller](controller.md)）
- 或在自定义协议里把 key 透传给 Channel

服务实例集合变化时（扩缩容），只有 `1/N` 的 key 会迁移，避免缓存全冷启。

### LoadBalancer 注册机制

```cpp
FLARE_DECLARE_CLASS_DEPENDENCY_REGISTRY(load_balancer_registry, LoadBalancer);

class MyBalancer : public LoadBalancer { /* ... */ };
FLARE_RPC_REGISTER_LOAD_BALANCER("my_strategy", MyBalancer);
```

注意 LoadBalancer 是 *class registry*（每个 cluster 一个实例），NameResolver 是 *object registry*（全局单例）—— 因为一个 resolver 可以服务多个 cluster，但一个 LoadBalancer 只属于一个 cluster。

## MessageDispatcher

[`flare::MessageDispatcher`](../rpc/message_dispatcher/message_dispatcher.h) 是 NSLB 整合模式的入口——一些路由服务（如腾讯内部的 CL5、L5、Polaris 等）自带"按名字得地址"和"负载均衡"两件套，再把它们拆成 NameResolver + LoadBalancer 反而绕路。

`MessageDispatcher` 一个接口把这两步合并：

```cpp
class MessageDispatcher {
  virtual bool Start(std::string_view scheme, std::string_view name) = 0;
  virtual bool GetPeer(std::uint64_t key, Endpoint* addr, std::uintptr_t* ctx) = 0;
  virtual void Report(...);
};
```

[`MessageDispatcherFactory`](../rpc/message_dispatcher_factory.h) 在构造 Channel 时按 URI scheme 决定：

- 注册了"整合式" dispatcher 的 scheme（如 `cl5://`）—— 用对应的 `MessageDispatcher`
- 其它 scheme —— 走 NameResolver + LoadBalancer 组合

调用方完全不用关心走的哪条路径。

## 缓存与刷新

`NameResolver` 自己实现缓存时，框架不再额外缓存；返回 `kUseGenericCache` 时，框架会用 [`NameResolverUpdater`](../rpc/name_resolver/name_resolver_updater.h) 在专用线程里周期 `GetPeers()` 拉新版本。频率可通过 gflag 调整。

`LoadBalancer` 自身不需要关心刷新——`SetPeers()` 被框架主动调用。

## 自定义示例

```cpp
// 自定义 zookeeper resolver
class ZkResolver : public NameResolver {
 public:
  std::unique_ptr<NameResolutionView> StartResolving(
      const std::string& name) override {
    return std::make_unique<ZkResolutionView>(name);
  }
};
FLARE_RPC_REGISTER_NAME_RESOLVER("zk", ZkResolver);

// 然后业务就可以用：
flare::RpcChannel channel;
channel.Open("zk://example.Service");   // 走 ZkResolver
```

无侵入扩展是设计目标——加新 scheme 不需要改框架核心。

---
[返回目录](README.md)
