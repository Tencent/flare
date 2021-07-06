# Fiber

本文档描述了fiber的基本设计及其使用。[fiber的调度相关设计在单独的文档中介绍。](fiber-scheduling.md)

在flare中，fiber，即用户态线程，作为底层的调度实体。fiber支撑（但不依赖）上层的[io](io.md)/rpc等逻辑。这类似于部分直接面向系统线程的框架对pthread的使用。

对于对brpc有使用经验的用户，可以将flare中的fiber和[bthread](https://github.com/apache/incubator-brpc/blob/master/docs/cn/bthread.md)做类比。

*brpc文档中将fiber限定为N:1的调度模型，这与我们这儿的（M:N的）定义不同。目前业界的fiber（folly/boost/...）实现中，两种调度模型的实现均存在。*

fiber通常对上层透明。

## fiber vs coroutine

flare底层基于[fiber](https://en.wikipedia.org/wiki/Fiber_(computer_science))开发，并[自行设计了一套fiber库](../fiber)。

这一节简单介绍了fiber与[协程（coroutine）](https://en.wikipedia.org/wiki/Coroutine)之间的区别及联系。

### fiber

以下内容节选自[Fiber (computer science) - Wikipedia](https://en.wikipedia.org/wiki/Fiber_(computer_science))：

> In computer science, a fiber is a particularly lightweight thread of execution.
>
> Like threads, fibers share address space. However, fibers use cooperative multitasking while threads use preemptive multitasking. Threads often depend on the kernel's thread scheduler to preempt a busy thread and resume another thread; fibers yield themselves to run another fiber while executing.

即，fiber是一种轻量的线程，也常被称为“纤程”、“绿色线程”等。其作为一个调度实体接收运行时的调度。

为方便使用，我们也提供了用于fiber的`Mutex`、`ConditionVariable`、`this_fiber::`、fiber局部存储等基础设施以供使用。

使用fiber编程时思想与使用pthread编程相同，均是使用传统的普通函数（这与下文中的coroutine形成对比）编写同步代码，并由运行时/操作系统负责在fiber/pthread阻塞时进行调度。

### coroutine

以下内容节选自[Coroutine - Wikipedia](https://en.wikipedia.org/wiki/Coroutine)：

> Coroutines are computer program components that generalize subroutines for non-preemptive multitasking, by allowing execution to be suspended and resumed. Coroutines are well-suited for implementing familiar program components such as cooperative tasks, exceptions, event loops, iterators, infinite lists and pipes.

即，协程是一种可以被挂起、恢复（多进多出）的函数（“subroutine”）。其本身是一种被泛化了的函数。

由于协程本质上依然是一个函数，因此其不涉及调度、锁、条件变量、局部存储等问题。

### 二者的联系

除[Coroutines (C++20)](https://en.cppreference.com/w/cpp/language/coroutines)及部分基于宏实现的协程库之外，大多数的实现基于切换运行时栈实现。

对于fiber，显然各个fiber作为一个执行实体，必然拥有各自独有的运行时栈。

但是这并不是说fiber和coroutine是相同或相似的。其二者有本质的区别。fiber本质上是一个调度实体，在fiber上可以执行普通函数，或协程（包括但不限于C++20的Coroutines或其他协程库）；而coroutine本质上是一个函数，取决于实现，其可能是stackful（需要切换栈）的，或者stackless（如C++20的Coroutines）的。

### 我们的选择

考虑到如下问题：

- 取决于协程库的实现，大多数协程库中**单个协程阻塞会导致对应的pthread关联的所有的协程的运行被延迟，造成响应时间毛刺**
- 除[asio](https://think-async.com/Asio/)外常见的支持用户态调度的RPC框架面对用户的最终形态均是单纯的栈切换而没有体现出协程自身独到的能力（多入多出等）
- 协程的学习成本（区分stackful vs stackless，理解多入多出）对于业务开发的同学更高
- 基于用户态栈切换实现的协程和C++20的协程作为完全不同的两种实现，易于混淆
- 基于fiber的锁、条件变量、局部存储等为业务代码优化提供了更多的空间
- fiber具有灵活的调度模型（N:1、M:N等。“协程”作为一个函数，本身不存在“调度”的概念。）
- fiber可以和io、rpc等上层逻辑结合并提供更多的优化空间

我们最终选择使用fiber来支撑我们的框架。

## pthread互操作性

fiber环境通常可以直接使用pthread相关原语，但是需要注意**避免在依赖线程上下文的环境中触发fiber调度**。如`std::mutex`加解锁通常需要在同一个线程中，如果加解锁之间触发了fiber调度，行为将是未定义的。

另一方面，在和pthread交互过程中可能还会涉及到如下问题：

- 等待pthread中的计算结果。
- 从pthread环境中启动fiber。

对于这些问题，我们将在后续几节进行解释。

### 等待pthread中的计算结果

通常我们不推荐业务代码自行创建线程池，因为通常对于单线程的业务逻辑而言，创建线程池并没有意义并且会（明显的）降低系统整体吞吐并导致时延升高、产生毛刺等。

如果业务逻辑需要并发计算，通常我们推荐使用[`Async`](async.md)来创建更多的fiber以获得并行计算的能力，如：

```cpp
std::vector<int> ComputeInParallel(const std::vector<Data>& datum) {
  std::vector<Future<int>> fs;
  for (auto&& data : datum) {
    fs.push_back(flare::Async([p = &data] { /* ... */ }));
  }
  return flare::fiber::BlockingGet(flare::WhenAll(&fs));
}
```

但是对于业务逻辑需要**单个请求**在**特定环境的**线程池中（可能并发）计算以改善计算速度的场景，在fiber和pthread之间传递数据是合理的。

我们提供了[`Future`](../base/future.h)来支持fiber环境和业务自行创建的pthread之间的交互能力。

我们的设计中，`Future<Ts...>`类本身不绑定到pthread或fiber，因此其可以自由的在fiber和pthread环境之间传递。

数据的传递通过`Promise<Ts...>::SetValue`进行。由于这一方法不阻塞，因此可以自由的在pthread或fiber环境中执行。

数据的等待最终通过`flare::BlockingGet(...)`或`flare::fiber::BlockingGet(...)`实现。其中前者使用pthread的同步原语实现，因此应当用在pthread环境中等待`Future<...>`（注：fiber环境中亦可使用，但是会阻塞底层pthread，影响整体性能）。后者使用fiber的同步原语实现，因此**只能**用在fiber环境中等待`Future<...>`，并且不会导致底层pthread阻塞，具有较好的性能表现。

因此，对于一个需要利用线程池并发计算的接口的实现而言，其实现通常类似于：

```cpp
void ComputingService::HeavyCompute(const ComputeRequest& req,
                                    ComputeResponse* resp,
                                    flare::RpcServerController* controller) {
  std::vector<Future<int>> fs;

  // Do computation in specialized thread pool.
  for (int i = 0; i != req.job_size(); ++i) {
    fs.push_back(specialized_thread_pool->Queue([job = req.job(i)] {
      // ...
    }));
  }

  // Wait until all computations are done.
  auto rcs = flare::fiber::BlockingGet(flare::WhenAll(&fs));

  // Fill the response.
  for (auto&& e : rcs) {
    resp->add_value(e);
  }
}
```

### 从pthread环境中启动fiber

某些情况下，如第三方代码在pthread环境主动调用回调创建事件，服务实现时可能会需要从pthread环境创建fiber以便于后续和flare交互。

对于这种情况，我们提供了[`StartFiberFromPthread`](../fiber/fiber.h)方法。这一方法内部会避免使用fiber相关上下文信息，并生成新的fiber放入调度队列以在fiber环境中调用用户的回调。

## 系统Fiber

除了面向用户的Fiber之外，我们系统中还有一种特殊的“系统Fiber”。

这种Fiber只在Flare框架内部使用。它和与面向用户的Fiber的主要区别[在于其运行时栈的分配](../fiber/detail/stack_allocator.h)：

- 栈大小硬编码为[`kSystemStackSize`](../fiber/detail/stack_allocator.h)。
- 没有Guard page。
- 对象池相关参数中这种Fiber的线程局部缓存更大。

这主要是考虑到系统代码对Fiber的使用有如下特点：

- 代码行为可控，因此栈大小可以进一步节省，且可以避免Guard page占用一个VMA。
- 因为没有了VMA限制，所以可以增大对象池的缓存，改善性能。

## 物理布局

我们的Fiber在内存中的物理布局如下：

```text
+--------------------------+  <- Stack bottom
| fiber control block      |
+--------------------------+  <- 512 byte
| ...                      |
| ...                      |  <- (Used stack space)
| ...                      |
+--------------------------+  <- Stack top.
| ...                      |
| ...                      |  <- (Unused stack space)
| ...                      |
+--------------------------+  <- Stack limit
| guard page (opt)         |  <- (User fiber only)
+--------------------------+  <- Stack limit + PAGE_SIZE
```

我们的Fiber控制块（[`FiberEntity`](../fiber/detail/fiber_entity.h)）保存在栈底（即VA的最大值，而不是`rsp`指向的栈顶），后面我们的[GDB插件](gdb-plugin.md)还原现场时需要这一数据。

根据[`--flare_fiber_stack_enable_guard_page`](../fiber/detail/stack_allocator.cc)是否启用，每个栈可能还会有一个不可访问的页用于检测栈溢出。

*启用guard page意味着每个栈有两个内存段（VMA），而不启用通常只需要一个VMA（但是有栈溢出检测不到的风险）。VMA在Linux上是一种受限制的资源，可以参考下文修改。*

## 调试

我们提供了GDB插件用于枚举进程中的fibers。具体使用及技术细节可参见[gdb-plugin.md](gdb-plugin.md)。

## 常见问题

这一节列出了传统的面对多线程环境的代码在fiber环境中可能出现的问题。

### `vm.max_map_count`过小导致fiber创建失败

每分配一个fiber栈会引入两个内存段（`/proc/self/maps`中表现为一行），它们分别作为fiber栈和用于检测栈溢出的[guard page](http://man7.org/linux/man-pages/man3/pthread_attr_setguardsize.3.html)。

Linux系统会限制每个进程所允许的最多的内存段的个数，默认值为65536。对于QPS高且单个请求处理时间长（*并发*请求量大）的服务，可能会达到这一上限（32768，考虑到对象池导致的线程局部缓存的栈对象以及其他代码、文件映射占用的内存段，上限大约是30K个fibers）。

这一参数可以通过修改[vm.max_map_count](https://www.kernel.org/doc/Documentation/sysctl/vm.txt)来解决。具体大小可以视业务需要修改。

修改方式：

```bash
echo 1048576 > /proc/sys/vm/max_map_count
```

这一选项的副作用可以参考[Side effects when increasing vm.max_map_count](https://www.suse.com/support/kb/doc/?id=7000830)，通常不会对服务及机器产生影响。

对于（包括但不限于）[运行Elasticsearch的环境，这一参数可能已经被增大了](https://www.elastic.co/guide/en/elasticsearch/reference/current/vm-max-map-count.html)，具体可以和运维同学确认。

### TLS、pthread mutex等依赖线程环境的逻辑

对于依赖线程上下文的操作，如访问线程局部存储（thread local storage，TLS）、加解锁（pthread mutex），需要保证在操作过程中不触发fiber调度（如避免发起RPC），否则行为将是未定义的。

对于TLS，可能的情况下，亦可以考虑使用`flare::FiberLocal<T>`进行相应的代码改造以适配fiber环境。

---
[返回目录](README.md)
