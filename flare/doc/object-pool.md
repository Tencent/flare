# 对象池

Flare 内部用了一些对象池来优化性能。

通常来说，关于对象的创建/释放，以下几种情况可能导致性能的退化或抖动：

- 创建、释放成本高：对象池对这类对象的效果显而易见，可以改善性能。
- 对象频繁在各个线程之间迁移：取决于内存分配器，多数依赖线程局部的缓存的内存分配器（如[tcmalloc](https://github.com/gperftools/gperftools)）对于这种情况，如果对象迁移导致了各个线程的局部缓存大小不均衡，会导致频繁进行全局的内存rebalance，这一操作通常需要加全局锁，影响伸缩性并引入明显的性能抖动。
  *特别的，[mimalloc](https://github.com/microsoft/mimalloc)对这类情况做了针对性优化并由明显的[性能收益](https://github.com/microsoft/mimalloc#benchmark-results)。*
- 工作集过大：取决于内存分配器，其可能会对线程局部缓存大小设定某个限制（如[tcmalloc的运行时参数](https://gperftools.github.io/gperftools/tcmalloc.html#runtime)）

对于flare而言：

- 第一种情况目前最典型的情况是fiber运行时栈的分配与释放（内部而言为了统一编码，fiber的栈也作为对象交由对象池管理）：栈分配及释放均需要多次syscall（包括但不限于`mmap`、`munmap`、`mprotect`），且初次分配后对栈的访问会引发pagefault并触发[demand paging](https://en.wikipedia.org/wiki/Demand_paging)，导致非常明显的性能退化。
- 第二种场景则更为普遍，第二种场景最常见的如生产者消费者场景，由于我们完成IO之后，需要在其他fiber中进行请求解析以及处理，往往会导致[缓冲区](io.md)、请求对象在创建后被迁移到执行解析请求、处理请求的fiber（对应的pthread）中释放。与之类似的还有rpc客户端的[定时器](timer.md)对象，其由各个rpc发起fiber分配，并统一在定时器线程中释放。取决于内存分配器，这些场景可能会引发明显的性能问题。

因此flare也设计了自己的对象池来保证性能的平稳。

目前我们提供了三种[“后端”实现](../base/object_pool.h)：

- [空实现](../base/object_pool/disabled.cc)
- [线程局部缓存](../base/object_pool/thread_local.cc)
- [NUMA节点内共享](../base/object_pool/memory_node_shared.cc)

为了使用线程池，需要特化`flare::PoolTraits<T>`，具体的使用方式及各个实现所需的参数可以[参考代码中的注释](../base/object_pool.h)。

## 空实现

这个实现主要是作为调试目的。

*使用这一实现需要将`PoolTraits<T>::kType`指定为`PoolType::Disabled`。*

对象池通常导致内存泄漏难以溯源（比如方法`A`分配后释放回线程池，之后方法`B`泄漏了对象，一些泄漏检查器会认为是方法`A`泄漏的）。因此我们提供了这一空实现，可以有选择的将某类对象的分配释放直接交给系统库来进行，方便调试。

## 线程局部缓存

这个实现严格的在线程内复用对象，因此其没有共享数据的同步开销，性能最高。但是另一方面，这个实现对于线程间频繁迁移且迁入迁出数据量不对等之类的情况不适用。

对于单线程执行业务逻辑（多线程用于并发处理请求，而不是多线程用于加速单个请求）的情况（大多数服务属于此类），如果各个请求的工作集及处理逻辑相似，可以优先考虑这一对象池。

*使用这一实现需要将`PoolTraits<T>::kType`指定为`PoolType::ThreadLocal`。*

*flare框架本身由于涉及大量的数据迁移，因此对这一对象池较少使用。*

就其[实现](../base/object_pool/thread_local.cc]而言，这种对象池至少会保留`PoolTraits<T>::kLowWaterMark`个对象存活，除此之外，如果当前线程局部的对象池数量尚未超过`PoolTraits<T>::kHighWaterMark`，那么对于被释放尚不足`PoolTraits<T>::kMaxIdle`时长的对象，会保留下来备用，否则将会被释放。

对象的重用使用LIFO的方式，即后释放的先重用，我们期望这有利于保持缓存热度。

对象的释放使用FIFO的方式，由`object_pool::Put<T>`间接触发，并且每轮的释放数量、两轮之间的时间间隔（内部记录时间使用[相对粗粒度（~10ms误差）的时间戳](timestamp.md)以提高性能）均有限制，避免负载突降产生现剧烈波动。

## NUMA节点内共享

这个实现包含了（可选）的线程局部缓存，但是在线程缓存为空或溢出时，会请求或释放给NUMA节点共享的对象缓存。

如果不确定应当使用何种对象池，这个对象池是一个可以满足需求的选择。

*使用这一实现需要将`PoolTraits<T>::kType`指定为`PoolType::MemoryNodeShared`。*

就其[实现](../base/object_pool/memory_node_shared.cc)而言：

- 每个线程局部会有至多`PoolTraits<T>::kMinimumThreadCacheSize`个对象的缓存
- 每次在共享缓存和线程局部缓存之间按照`PoolTraits<T>::kTransferBatchSize`个对象的大小来迁移
- 共享缓存至少会保留`PoolTraits<T>::kLowWaterMark`个对象
- 共享缓存分块中超过`PoolTraits<T>::kHighWaterMark`数量、或存活时间超过`PoolTraits<T>::kMaxIdle`的对象会被释放

对象的重用及释放同样是LIFO/FIFO的方式，并且有逻辑控制释放频率保证整体性能稳定（这可能导致存货对象个数临时超过上限）。

---
[返回目录](README.md)
