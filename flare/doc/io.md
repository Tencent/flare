# IO

这篇文档主要描述flare在IO方面的逻辑。

## 事件循环

我们每个[调度组](fiber-scheduling.md)内都有一个或多个事件循环（[`epoll_wait`](https://man7.org/linux/man-pages/man2/epoll_wait.2.html)）。

*每个调度组内的事件循环个数取决于`flare_event_loop_per_scheduling_group`。默认为1，绝大多数情况下都应当足够，**不建议盲目修改**。若调度组一个事件循环不够时建议优先考虑减小调度组大小。*

事件循环同样作为[fiber](fiber.md)执行，但是事件循环对应的fiber在创建时通过特殊参数（[`Fiber::Attributes::scheduling_group_local`](../fiber/fiber.h)）加了特殊标记，避免其被任务偷取到其他的调度组内。

事件循环仅负责从系统中获取各个fd上的事件，实际的IO操作会创建独立的Fiber进行。这有助于提高不同连接上的IO并行度，避免事件循环自身成为瓶颈，并且（明显的）改善延迟并提高吞吐。

显然，由于我们的[调度策略](fiber-scheduling.md)，通常**事件循环、IO、以及IO的后续处理，都会在同一个调度组内进行**。但是这也意味着**服务端在连接数量足够少的情况下（小于调度组个数时），会有调度组没有工作负载**。考虑到通常我们线上环境均有一定数量的上游足以超过调度组个数（通常为数个），因此在实际使用中这应当不是一个问题。如果确实连接数非常少时，可以考虑增大调度组大小。

### 看门狗

考虑到事件循环对整个服务至关重要，我们增加了[`Watchdog`](io/../../io/detail/watchdog.h)来定期检测事件循环是否正常。

实现而言，我们通过投递任务至事件循环并等待其执行来判定事件循环是否正常。如果投递的任务长期没有执行，我们认为事件循环没有响应。

具体的检测逻辑可以通过如下参数控制：

- `--flare_watchdog_check_interval`：两次检测的时间间隔（毫秒）。
- `--flare_watchdog_maximum_tolerable_delay`：最大允许的投递任务至执行的延迟（毫秒）。为了避免系统繁忙时误认为无响应，我们建议设置一个相对较大的延迟容忍。
- `--flare_watchdog_crash_on_unresponsive`：如果指定为`true`，在检测到事件循环无响应是`Watchdog`会通过`SIGABRT`终止整个进程（取决于运维系统，之后程序可能会被运维系统重新启动继续提供服务。）。否则会输出类似于`Event loop 0xXXXXXXXX is likely unresponsive. Overloaded?`的错误日志。

## 非连续缓冲区

由于我们的IO、上层处理等均在不同的fiber中进行，为了维护上下文往往需要复制缓冲区，因此我们使用了一套[非连续的缓冲区](../base/buffer.h)的设计。

我们的缓冲区实际上是对一系列数据块进行了引用计数并记录了这个缓冲区引用了每个数据块中的哪一段。复制时只要增加引用计数即可。

*由于我们的处理逻辑通常在同一个调度组内进行，而调度组通常绑定至某个NUMA节点，因此引用计数通常**不**会引发昂贵的跨节点的缓存连贯性消息。更多关于原子更新引用计数导致的性能成本的讨论，参见[性能导引](performance-guide.md)。*

另外，为了方便和外界的缓冲区整合（如用户自己分配的[`std::string`](https://en.cppreference.com/w/cpp/string/basic_string)、或是直接引用[`mmap`](https://man7.org/linux/man-pages/man2/mmap.2.html)映射的文件等等），我们还提供了一系列的方法来包装这些外界的缓冲区，具体使用可以参考[缓冲区](buffer.md)。

## IO操作

IO相关操作方面我们学习并借鉴了[brpc](https://github.com/apache/incubator-brpc/blob/master/docs/cn/io.md)的一些相关实现。

事件循环检测到IO事件之后，会对不同的IO事件分别创建不同的Fiber来执行，因此每个fd上的读、写操作都是并发进行的。

为了避免IO事件遗漏，我们会记录每个事件的发生次数做原子计数，每次遇到`EAGAIN`后递减，若不为0则立即重试。显然这可能导致不必要的重试，但是相对于独立Fiber进行IO带来的延迟方面的收益，我们认为这是可以接受的。

在数据读取之后，会被交由上层的rpc逻辑进行[协议解析](protocol.md)并调用业务代码处理。

## 无锁写出

一系列写出缓冲区链表本质上是一个多生产单消费的队列。生产者将需要写出的缓冲区追加至队列，并由消费者负责通过[`writev`](https://linux.die.net/man/2/writev)将至交给系统。

我们使用一套从[mcs lock](https://lwn.net/Articles/590243/)改造而来的无锁的MPSC队列。

*brpc中也有一套[类似的算法](https://github.com/apache/incubator-brpc/blob/master/docs/cn/io.md#%E5%8F%91%E6%B6%88%E6%81%AF)。*

我们维护一个`tail`指针，标记当前队列的尾部。

当有数据追加时，我们创建新的节点（其`next`指针为`nullptr`），并将其将新的数据和`tail`做原子交换，此时根据交换前的`tail`的值，有两种情况：

- 交换前`tail`为`nullptr`，这时我们的缓冲区是队列中的第一个，因此我们可以宣告对这个队列的*消费*的**唯一**所有权。
- 交换前`tail`不为`nullptr`，此时我们更新交换前的`tail->next`为我们的节点。

如果交换前的`tail`不为`nullptr`，那么这种情况属于追加数据的情况，至此便可以返回了。

否则我们调用方通过`writev`开始消费缓冲区链表。在消费过程中可能出现如下几种情况：

- 系统缓冲区满：更新`head`指针（因为只有一个线程可能会写出，而目前队列还没被写完因此不会有其他人成为新的队列所有者，因此这个操作是安全的），等待`EPOLLOUT`后由事件循环创建fiber继续写出
- 缓冲区链表太长或待写出的缓冲区太大，写了很久都没有完成：我们可以直接创建fiber并继续写出（brpc采取了这种做法），但是为了简化编码，我们采取了和系统缓冲区满相同的做法——启用`EPOLLOUT`（因为系统缓冲区没满所以会立即触发）然后由事件循环创建fiber写出
- 队列*疑似*写完，即当前节点的`next`指针为`nullptr`，此时将当前的节点和`tail`做`cmpxchg`试图将`tail`改为`nullptr`：
  - `cmpxchg`成功：队列耗尽，返回。
  - 否则`tail`指针已经被更新了，但是可能当前节点的`next`其尚未来得及更新（生产者更新`next`在更新`tail`之后），盲等直到`next`被更新为止，然后继续写出。

## 连接池

目前，针对支持链路复用的协议，我们会在每个[调度组](scheduling-group.md)针对每种协议维护一个连接池。

连接池的大小受[`flare_rpc_client_max_connections_per_server`](../rpc/internal/stream_call_gate_pool.cc)控制。内部而言，我们会将这个配置向上取整后等分至各个调度组。

针对实际应用场景，我们还做了如下优化：

- 低负载时避免创建过多连接：这一优化可能导致实际程序创建的连接数大不到配置的上限。这是考虑到在实际应用场景中，如果QPS足够低，连接数过多的情况下可能单个连接上两次请求的间隔会很大。而在我们的环境中（内网延迟较小），通常间隔200ms就会因[`tcp_slow_start_after_idle`](https://www.kernel.org/doc/Documentation/networking/ip-sysctl.txt)而导致重新执行慢启动，引入不必要的延迟。

---
[返回目录](README.md)
