# Redis

Flare 提供了内置的 [Redis 客户端](../../net/redis/redis_client.h)。

这一客户端可以用于请求官方的 [Redis](https://redis.io/) 服务端。

我们将Redis操作统一通过如下类型包装：

- [`RedisCommand`](../../net/redis/redis_command.h)：`RedisCommand` 用来构造Redis的请求。之后可以通过 [`RedisClient`](../../net/redis/redis_client.h) 请求 Redis 服务器。

- [`RedisObject`](../../net/redis/redis_object.h)：这一类型用于描述 Redis 的回复。Redis 可能的回复类型可以参考 [Redis 协议文档](https://redis.io/topics/protocol)。这个类型提供了 `is<T>` 和 `as<T>`进行类型检查及转换至如下类型：

  - `RedisString`：对应于 Redis 文档中的“Simple Strings”。
  - `RedisInteger`：对应于 Redis 文档中的“Integers”。
  - `RedisError`：对应于 Redis 文档中的“Errors”。另外，`RedisError` 也会用来标记我们内部报的错误，这种情况下，“错误类型”会增加`X-`前缀以作区分。
  - `RedisBytes`：对应于 Redis 文档中的“Bulk Strings”。
  - `RedisArray`：对应于 Redis 文档中的“Arrays”。
  - `RedisNull`：对应于 Redis 文档中的“Nil”。

TODO(luobogao)：我们欠缺将 Redis 协议层对象转换为应用层对象（Hash 等）的能力。

## 使用

通常可以直接构造 [`RedisClient`](../../net/redis/redis_client.h) 来使用。URI格式形如 `redis://192.0.2.1:4567`。

`RedisClient` 提供如下方法：

- `Execute`：接收一个 `RedisCommand` 对象，同步等待（阻塞 [fiber](../fiber.md) 但不阻塞 pthread）并返回 `RedisObject`。
- `AsyncExecute`：同 `Execute`，但返回 `Future<RedisObject>`。

具体的使用也可以参考我们[单测文件](../../net/redis/redis_test.cc)中的使用示例。

与其他客户端类似，`RedisClient`可以从 URI 构造或者使用一个已有的 [`RedisChannel`](../../net/redis/redis_channel.h) 发送命令。

需要注意的是，目前我们的 `RedisClient` / `RedisChannel` 构造成本较高，因此建议在可能的情况下复用已有的 `RedisClient` / `RedisChannel`。

### 通过 URI 请求 Redis 服务器

```cpp
// Polaris address is recognized too.
flare::RedisClient client("redis://127.0.0.1:6379");
auto result = client.Execute(flare::RedisCommand("GET", "some_key"));

// Test if `result` is a `flare::RedisBytes` object and use it.
```

### 通过已有的 `RedisChannel` 请求 Redis 服务器

```cpp
// Polaris address is recognized too.
flare::RedisChannel channel("redis://127.0.0.1:6379");
flare::RedisClient client(&channel);
auto result = client.Execute(flare::RedisCommand("GET", "some_key"));

// Test if `result` is a `flare::RedisBytes` object and use it.
```

## 测试

关于依赖于 Redis 的单测，可以参考[测试指引文档中 Redis 一节](../testing.md)。

## 未实现的功能

如下功能目前我们没有支持：

- Pipelining
- Pub/Sub channel
