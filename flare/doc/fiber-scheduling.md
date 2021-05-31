# Fiber 调度

这篇文档主要描述我们的fiber调度相关的逻辑及策略。

## 上下文切换

flare使用修改版的[boost.context](https://github.com/boostorg/context)进行上下文切换。

根据ISA的不同，我们的修改版的boost.context位于：

- [x86-64](../fiber/detail/x86_64/)
- [ppc64le](../fiber/detail/ppc64le/)

## 调度

flare中fiber和pthread之间采用M:N的模型来进行调度。

*相对于N:1以及常见的协程（Fiber和协程的区别见[fiber.md](fiber.md)）设计，M:N会引入更多的cache miss，导致性能下降。这是我们为了保证性能（响应时间等）的平稳性所难以避免的代价。*

*但是这并不是说我们的[性能表现](benchmark.md)不如其他N:1的框架或协程框架。系统的整体性能实际上可以由[多方面进行优化](performance-guide.md)。单纯的调度模型并不会决定一个框架的整体性能。*

我们会启动*不少于*`flare_concurrency_hint`个线程来执行fibers。我们的实现中，实际创建的线程数可能大于`flare_concurrency_hint`。后文将会做更进一步解释。

### 配置参数

根据调度的配置参数不同，可能出现如下两种问题：

- 调度组（见后）过小导致组间负载不均衡，浪费CPU。
- 调度组过大导致组内共享的数据结构竞争加剧。

通常而言，我们建议用户通过如下参数向框架描述负载情况并由框架自行确定调度相关参数：

- `flare_fiber_scheduling_optimize_for`：指定负载类型（CPU密集或网络密集），可选项：
  - `compute-heavy`：计算密集。框架将尽可能保证各个处理器之间的负载均衡。
  - `compute`：计算密集，但是框架在确定参数时不会像`compute-heavy`那么激进。
  - `neutral`：默认配置，应当可以满足大多数使用场景。
  - `io`：IO（特指通过Flare进行的，不包括直接读写磁盘等Flare感知不到的场景）密集。这种场景下各个请求处理时间较短，因此高负载下QPS很高，组间少量不均衡的影响小于共享数据竞争的影响。这种情况下框架确定参数时优先考虑降低数据竞争。
  - `io-heavy`：同`io`，但是更加激进。

由于这一参数通常由一个服务的业务特点决定，是相对恒定的，因此通常可以考虑通过[`FLARE_OVERRIDE_FLAG`](gflags.md)在代码中直接设定。

## 调度组

出于性能考虑，flare中对底层的pthread workers进行了分组，每一个分组我们称之为一个[调度组](scheduling-group.md)，fiber*通常*在一个组内的pthread workers之间迁移。

我们的设计允许各个调度组之间以任务偷取的方式迁移fiber，但是任务偷取的频率是受到限制的。隶属于不同NUMA节点的调度组之间的任务偷取默认被禁用，如果手动启用，其频率也会受到相对于同NUMA节点之间而言，更进一步的限制。

调度组的大小取决于参数`flare_scheduling_group_size`，且不能超过64。

每个调度组内至多允许有`flare_fiber_run_queue_size`个可执行的fibers。默认值通常可以满足业务需求，对于极端情况，可自行修改。这一参数必须是2的整数次幂。

在如下几种情况下，我们会将各个调度组的亲和性分别关联到某个不同的NUMA域（中的所有CPU）：

*对于多线程之间的同步操作较少的负载而言，设置亲和性有助于改善其整体吞吐及时延。*

- 启动程序时指定了`flare_numa_aware`参数为`true`
- `flare_numa_aware`未指定且以下两条均成立：
  - 进程启动时未被指定CPU亲和性
  - `flare_concurrency_hint`大于CPU个数，分组后调度组个数不小于NUMA节点数

关于调度组及其相关的参数对性能的影响，参见[调度组](scheduling-group.md)。

## pthread workers唤醒算法

fiber从被创建（或被唤醒）到被pthread执行是一个典型的生产消费的场景。通常我们有如下办法可以解决：

- 空闲的工作线程轮询队列：有助于改善唤醒延迟，生产者（fiber的创建方）不需要发起syscall唤醒工作线程。由于一直在轮询，CPU负载持续100%，通常的生产环境中不可接受（否则直接上DPDK之类的轮询框架就可以了）
- 共享队列、条件变量：共享数据结构竞争激烈，共享队列插入时不加锁可能导致wakeup loss，加锁对整体吞吐影响大。无法避免唤醒工作线程的syscall
- 每个工作线程一个队列：容易造成负载不均衡，唤醒工作线程syscall
- 每个工作线程一个队列、任务偷取：取决于任务偷取强度，（强度低）可能无法解决不均衡，（强度高）或造成低负载下大量无效偷取导致的各个队列的锁竞争，唤醒工作线程syscall

由于上述算法均有较为明显的缺陷，因此我们自行设计了唤醒算法。

*显然这儿的算法是针对某个调度组而言，因此其共享的状态均是调度组内的pthread workers之间共享，且pthread个数可控（为`flare_scheduling_group_size`）。*

### 状态

我们维护如下（调度组内）共享状态：

- run queue：等待被执行的fibers均会被放置于此，无锁的有界（大小取决于`flare_fiber_run_queue_size`）队列。
- sleeping mask：64位整数，每一位对应一个pthread worker。用途见后。
- spinning mask：64位整数，每一位对应一个pthread worker。用途见后。
- pending wakeup: bool。用途见后

对于每个线程，我们还维护如下数据：

- wait slot：通过[futex](http://man7.org/linux/man-pages/man2/futex.2.html)实现，pthread worker无工作时在此休眠，并可由其他人将至唤醒。

#### 有界对列和无界队列

初看起来，我们这儿run queue使用有界对列会为用户引入不必要的可能需要调节的参数（`flare_fiber_run_queue_size`）。

但是实际上，相对于无界队列，有界队列有如下优势：

- 无锁实现不用考虑内存释放问题，易于实现。
- 实现方式多，数组、链表等，在性能上更多的优化空间。
- 性能表现平稳，不会出现内存分配导致的偶发延迟。

同时，无界队列虽然可以避免需要手动调节参数，但是在实际情况中，考虑到：

- 每个调度组只有一个队列，且每个队列占用内存大小为`flare_fiber_run_queue_size * sizeof(FiberEntity*)`，即便我们预设一个很大的值（如1M），每个调度组也只有队列节点大小*1M的开销。
- 系统中可以同时存在的fiber数量制约因素很多，远非这一个参数可决定（如[`vm.max_map_count`](fiber.md)、用于映射栈的物理内存大小等）。
- 如果系统中真的存在海量的fiber需要执行，通常意味着系统已经出现了其他方面的瓶颈。

因此通常一个足够大的预设值可以满足绝大多数环境；当预设值不足时程序本身往往已经不能正常运行了，此时无界队列自动扩容带来的好处很少有实际场景。

因此我们选择了有界队列作为我们的run queue。

### 算法

我们对生产者（创建、唤醒fiber的线程）和消费者（空闲的等待新fiber来执行的线程）分开描述。

#### 空闲线程

##### 轮询队列

当一个pthread worker从队列中取不到fiber（即队列为空）后，会检测spinning mask。这一字段记录了当前正在轮询run queue的pthread workers。

如果当前其中置1位的个数（[内部](../fiber/detail/assembly.h)通过**一条**[popcnt](https://www.felixcloutier.com/x86/popcnt)（SSE4）指令实现）小于2，则CAS（compare-and-swap）将自己对应的位置1（失败时重试）。否则转入休眠。

这样保证了至多调度组内只有2个线程在盲等，避免低负载时过多的CPU消耗在盲等上。

如果pthread worker轮询超时之后仍然取不到fiber，转入休眠。

如果pthread worker在轮询阶段取到了fiber，在离开之前会将pending wakeup置为true。此时另一轮询的pthread worker（如果有）发现这一字段改为true之后，会唤醒（唤醒逻辑见后文）一个新的pthread worker来轮询。这样可以提前唤醒pthread worker，在持续有新的fiber生成时，尽量保证有已经被唤醒的pthread worker可以直接执行。**即改善了fiber的调度延迟，又可以避免下一次有fiber可执行时，生产方的唤醒pthread worker的syscall成本。**

获取fiber之后返回时，pthread worker会将对应的spinning mask改为0（可能已经为0，见下文）。

##### 转入休眠

pthread worker会在休眠前后，对sleeping mask进行对应的更新。

#### 生成fiber的线程

在将fiber加入run queue之后，生产者首先检查spinning mask，如果不为0，通过[`__builtin_ffsll`](https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html#index-_005f_005fbuiltin_005fffsll)找到最低的置1位（通常编译为**一条**[bsf](https://www.felixcloutier.com/x86/bsf)指令），并将之（通过原子cas）置0。通过原子将之改为0，我们可以确信不会有其他的生产线程也误以为这个pthread worker会执行它的fiber，即我们以这一cas宣告了“所有权”。与此同时，对应的轮询线程应当已经或即将从run queue中获取我们刚加入的fiber。因此**生产者无需syscall而可以直接返回。**

否则生产者通过`__builtin_ffsll`找到sleeping mask的最低置1位并将之改为0（cas操作，同上），之后通过相应的wait slot将之唤醒。

这儿**我们始终优先考虑编号更低（在mask中对应更低位）的线程，负载较低时这会将负载集中在特定的几个线程上**。这有如下一些好处：

- 即便配置了过多的线程，因为负载不会派发到这些线程，对整体性能影响不大。
- 有助于将负载集中在cache更热的CPU
- 对于系统的调度（避免线程反复在CPU间迁移）及节能等考虑也都是有益处的（可以关闭无负载的核）。

---
[返回目录](README.md)
