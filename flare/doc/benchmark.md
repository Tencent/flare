# 性能数据

**此处数据均只代表测试当时的性能，现在情况可能已经发生改变。**

首先需要说明的是，对于**我们的**线上业务而言，单纯的Echo性能没有意义。因此flare更关注的是系统整体表现的平稳性（延迟等），而不是极限情况下的Echo的QPS。

我们在设计中亦存在着多处牺牲极限情况QPS换取延迟平稳的权衡，如[Fiber调度模型（M:N vs N:1）](fiber-scheduling.md)的选择等。

当然，在不影响性能平稳的前提下，我们依然做了许多的[性能优化](performance-guide.md)以尽可能的减少框架本身的性能开销。

## 2020-05-22（版本72920c01）

这次的测试和2020-01-09大体相同，主要用于对比近期功能迭代及性能优化对整体性能的影响。

测试环境同2020-01-09。

服务端命令行：`./server --logtostderr --flare_concurrency_hint={线程数}`。
客户端命令行：`./press --server_addr={协议}://127.0.0.1:5567 --logtostderr --flare_rpc_client_max_connections_per_server={线程数} --max_pending={线程数}*10 --flare_concurrency_hint={线程数}`。

*客户端、服务端均40线程时超过了硬件并发度（76C），因此存在少量抖动。*

*CPU跑不满主要是受限于客户端的最大并发数。考虑到最大并发数过大通常意味着请求已经开始堆积，线上正常运行时通常不会发生这种情况，因此我们没有过度的增加最大并发请求数。*

### flare协议

|线程数         |10   |20   |30   |40   |
|---------------|-----|-----|-----|-----|
|CPU（服务端）% |870  |1800 |2600 |3100 |
|CPU（客户端）% |920  |1850 |2700 |3200 |
|QPS            |650K |1.35M|1.75M|1.9M |
|延迟(us, avg)  |147  |145  |165  |200  |
|延迟(us, p999) |300  |550  |500 |3K    |

### http协议

|线程数         |10   |20   |30   |40   |
|---------------|-----|-----|-----|-----|
|CPU（服务端）% |900  |1800 |2800 |3300 |
|CPU（客户端）% |930  |1860 |2800 |3400 |
|QPS            |550K |1M   |1.45M|1.5M |
|延迟(us, avg)  |170  |180  |200  |270  |
|延迟(us, p999) |310  |500  |1K   |4K   |

## 2020-01-09（版本`b45ce61c`）

### Echo性能

这一节给出了单纯的Echo性能。其中服务端客户端均使用flare框架。

#### 测试配置

硬件配置如下：

- CPU: Xeon Gold 6133 @ KVM (76 vCPU, Hyper-threaded, 2 NUMA Node)
- RAM: 251G

软件设置如下：

- 同机测试
- 客户端每连接上维持10个待处理请求
- [调度组](scheduling-group.md)大小为10
- 客户端连接数与工作线程数相同
- 服务端及客户端工作线程数见后
- 协议见后

服务端命令行：`./server --logtostderr --ip=0.0.0.0 --flare_numa_aware --flare_concurrency_hint={线程数} --flare_scheduling_group_size=10`。`flare_numa_aware`对于调度组个数为奇数（不能被NUMA结点数整除）时不开启（否则flare会将线程个数向上对齐直至调度组个数可以被NUMA结点数整除）。
客户端命令行：`./press --server_addr={协议}://127.0.0.1:5567 --logtostderr --flare_rpc_client_max_connections_per_server={线程数} --max_pending={线程数}*10 --flare_numa_aware --flare_concurrency_hint={线程数} --flare_scheduling_group_size=10 --override_nslb=list+rr`。`flare_numa_aware`参数同上。

*测试程序位于[example/rpc](../example/rpc)。*

#### 测试数据

数据均来源于测试开始30s之后的10s内平均水平。

flare协议（`server_addr`使用`flare://127.0.0.1:5567`）：

|线程数         |10   |20   |30   |40   |
|---------------|-----|-----|-----|-----|
|CPU（服务端）% |900  |1800 |2700 |3200 |
|CPU（客户端）% |950  |1850 |2800 |3400 |
|QPS            |540K |1M   |1.2M |1.5M |
|延迟(us, avg)  |180  |195  |230  |258  |
|延迟(us, p999) |400  |800  |1.1K |4K   |

HTTP协议（`server_addr`使用`http+pb://127.0.0.1:5567`）：

|线程数         |10   |20   |30   |40   |
|---------------|-----|-----|-----|-----|
|CPU（服务端）% |930  |1900 |2750 |3250 |
|CPU（客户端）% |950  |1900 |2800 |3350 |
|QPS            |480K |970K |1.08M|1.2M |
|延迟(us, avg)  |206  |200  |280  |330  |
|延迟(us, p999) |450  |600  |1.3K |6K   |

应当承认，受限于目前flare的RPC客户端的实现，伸缩性及延迟（初步分析主要来源于flare的RPC客户端实现）依然有一定的改善空间，但仍明显优于现有框架（见后）。

*注：上述性能非极限性能，而是一个“相对合理的参数调优”下的结果。我们认为通过反复调节参数得出的“极限性能”对业务而言并无意义。如通过指定`--flare_numa_aware --flare_concurrency_hint=32 --flare_scheduling_group_size=8`（这一参数对Echo类接口友好，但是对相对复杂的业务逻辑则未必并且可能需要随着业务迭代反复调节），客户端40线程40连接3200并发请求可以在上述测试环境下用flare协议得到2M QPS、2ms avg. latency的结果，但是这种数据对实际业务鲜有参考价值。*

#### 与现有框架横向对比

为与现有框架对比，服务端使用`gdt_server`，参数同上，客户端增加参数`--gdt_stub`。

现有框架不支持二进制协议（除不再推荐新业务接入的的QZone协议），故只对比HTTP协议。

我们同时对比了现有框架开启线程池（`gdt-tp`）与不开启线程池（`gdt`）的情况。

需要注意的是：

- 不开启线程池时（`gdt`模式），Echo逻辑直接在IO线程中执行。尽管这样做QPS有明显优势，但由于这会导致对实际业务场景会下延迟非常不稳定（见“耗时不稳定的请求”一节），因此**不开启线程池的`gdt`模式，线上并不使用。此处仅作对比。**
- **对于生产环境，`gdt-tp`的数据更贴近实际情况。**
- `flare`由于业务逻辑始终不会阻塞IO，因此不涉及到“是否启用线程池”的区别。事实上，自行创建线程池，在flare环境中除了引入更多的编码复杂度并**降低**性能外，除少量特定场景，通常无额外收益。

测试参数：

- 客户端均使用`./press --server_addr=http+pb://127.0.0.1:5567 --logtostderr --flare_rpc_client_max_connections_per_server=40 --max_pending=400 --flare_numa_aware --flare_concurrency_hint=40 --flare_scheduling_group_size=10 --override_nslb=list+rr`
- 老框架启用线程池时IO线程和线程池大小相同（如40线程环境下意味着20 IO，20 worker）。

QPS：

|线程数         |10   |20   |30   |40   |
|---------------|-----|-----|-----|-----|
|flare          |570K |980K |1.2M |1.25M|
|gdt            |600K |980K |1.08M|1.1M |
|gdt-tp         |450K |700K |850K |840K |

延迟（us，avg）：

|线程数         |10   |20   |30   |40   |
|---------------|-----|-----|-----|-----|
|flare          |690  |400  |330  |330  |
|gdt            |680  |400  |300  |300  |
|gdt-tp         |850  |600  |500  |500  |

延迟（us，p999）：

|线程数         |10   |20   |30   |40   |
|---------------|-----|-----|-----|-----|
|flare          |1.2K |1.2K |2.5K |5K   |
|gdt            |8K   |6K   |3K   |5K   |
|gdt-tp         |8K   |8K   |8K   |8K   |

注：

1. 线程数更少时延迟更高应当是由于请求过多排队导致。
2. `gdt-tp`性能表现波动较大（~20% QPS波动），因此`gdt-tp`的相关数值均有一定的误差存在。`flare`及`gdt`不存在此问题。

可以看出：

- `flare`的QPS表现优于`gdt`，大幅领先于`gdt-tp`。
- `flare`的平均延迟延迟表现和`gdt`略高于`gdt`，优于`gdt-tp`。
- `flare`的p999延迟大幅领先于`gdt`及`gdt-tp`。

### 耗时不稳定的请求

我们在服务端的处理逻辑通过[`std::this_thread::sleep_for`](https://en.cppreference.com/w/cpp/thread/sleep_for)模拟业务由于各种原因（如IO，或有些业务根据请求不同而需要不同计算量）导致的处理延迟，以此在一定程度上模拟业务环境下flare的性能平稳方面的表现。

这一测试主要关注少量长连接、单个连接并发请求多的场景下业务逻辑耗时不均的场景下的框架表现。

*此处模拟不能使用[`fiber::SleepFor`](../fiber/this_fiber.h)。尽管我们通常建议业务在需要休眠时使用`fiber::SleepFor`，但对于这儿的**模拟延迟**的场景，由于`fiber::SleepFor`会直接将pthread worker交给其他fiber，因此并不能模拟阻塞。*

我们通过如下代码生成延迟（均值为50us的[泊松分布](https://zh.wikipedia.org/wiki/%E6%B3%8A%E6%9D%BE%E5%88%86%E4%BD%88)）：

```cpp
void ILoveWork() {
  thread_local std::mt19937 gen{std::random_device{}()};
  thread_local std::poisson_distribution<> dist(50);

  // No I actually take a nap.
  std::this_thread::sleep_for(1us * std::min(dist(gen), 1000));  // 1ms at most.
}
```

我们的测试参数如下：

- 服务端延迟：服从均值为50us的泊松分布
- 并发连接数：10
- 单个连接上并发请求数：100

客户端命令行：`./press --gdt_stub --server_addr=http+pb://127.0.0.1:5567 --logtostderr --flare_rpc_client_max_connections_per_server=10 --max_pending=1000 --flare_concurrency_hint=20 --flare_scheduling_group_size=10 --override_nslb=list+rr --flare_numa_aware`。

#### 与现有框架横向对比

*由于这一测试对IO压力不大，因此`gdt-tp`的IO线程和worker线程采用2:8的比率，这也符合通常线上IO线程明显少于worker的场景。*

QPS：

|线程数         |10   |20   |30   |40   |
|---------------|-----|-----|-----|-----|
|flare          |75K  |146K |185K |245K |
|gdt            |77K  |77K  |77K  |77K  |
|gdt-tp         |66K  |130K |190K |258K |

延迟（ms，avg）：

|线程数         |10   |20   |30   |40   |
|---------------|-----|-----|-----|-----|
|flare          |13   |6.7  |5.4  |4.1  |
|gdt            |13   |13   |13   |13   |
|gdt-tp         |14   |7.6  |5.2  |3.8  |

延迟（ms，p999）：

|线程数         |10   |20   |30   |40   |
|---------------|-----|-----|-----|-----|
|flare          |20   |16   |17   |9    |
|gdt            |46   |47   |48   |45   |
|gdt-tp         |90   |60   |70   |38   |

可以看到：

- `gdt`面对这种场景表现很差。由于一个fd只能被一个事件循环（对应一个线程）处理，而处理过程中又会阻塞后续请求的读取及处理，因此增加线程数也无法改善QPS及延迟。
- `gdt-tp`面对这种情况在调整了线程数之后表现有明显改善，但是：
  - 线程数需要业务手动试验性调整，且可能需要随着业务逻辑迭代定期review。
  - p999延迟改善并不明显。
  - 线程数较少时p999比`gdt`更大，这个主要是因为IO线程不用于业务逻辑（不能用来执行`ILoveWork()`），因此排队时间更长。
- `flare`的QPS及延迟表现均较为稳定，p999相对于`gdt`、`gdt-tp`有明显优势，同时：
  - 相对于`gdt-tp`：业务无需关注IO/worker线程比例（自然也就不需要定期review）
  - 相对于`gdt`：业务无需自行维护线程池相关逻辑即可获得出色的延迟稳定性

---
[返回目录](README.md)
