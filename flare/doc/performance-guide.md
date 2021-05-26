# 性能指引

这篇文档罗列了一些常见的需要注意的性能相关的点。这篇文档的目标群体主要为flare的开发人员。

## 一些独立列出的问题

- 调度组：我们通过分组的设计提高系统的伸缩性。这一设计单独描述于[这篇文档](scheduling-group.md)。

  简单来说，引入被多线程共享的数据时，需要优先考虑能否按照调度组进行分组共享。

- 断言/日志：应当首先考虑使用[我们提供的版本](logging.md)，而不是glog相同功能的宏。

- 对象池：对于构造析构成本高或频繁在线程之间迁移等通用目的的内存分配器不能很好的解决的场景，我们通过对象池进行优化。我们的对象池单独描述于[这篇文档](object-pool.md)。

  对于框架而言，大多数情况下“NUMA节点内共享”是一个比较适合的对象池实现。

- 时间戳：取决于具体场景的需要，我们可能需要视情况选择不同的时间戳。具体的决策逻辑参见[这篇文档](timestamp.md)。

- Hazard pointer：对于读多写少的场景，可以考虑使用[Hazard pointer](../base/hazptr/README.md)。

## 原子变量

**原子变量本身不是伸缩性瓶颈。**

不含内存屏障（对于C++，即`std::memory_order_relaxed`方式进行的）读或写（但[RMW](https://en.wikipedia.org/wiki/Read%E2%80%93modify%E2%80%93write)除外）的成本通常与普通变量无异。

取决于具体ISA，原子变量的RMW成本可能出现在以下两方面：

- 其暗含的内存屏障。取决于ISA，对于x86-64而言，原子变量的RMW均暗含完整的内存屏障。这会导致[CPI](https://en.wikipedia.org/wiki/Cycles_per_instruction)升高，但是其本身并不是一个*伸缩性*的问题。比如每个线程自增一个不同的原子变量，在不存在[false sharing](https://en.wikipedia.org/wiki/False_sharing)的前提下这是可以线性伸缩的。

  *特别的，在x86-64下，暗含内存屏障的[原子操作往往快于`mfence`](../base/internal/memory_barrier_benchmark.cc)（[但是不能作用于non-temporal的内存操作](https://stackoverflow.com/questions/40409297/does-lock-xchg-have-the-same-behavior-as-mfence)）。尽管我们非常不建议在C++中使用非`std::atomic<T>`系的方式添加内存屏障，但是必要的情况下这可以用于[优化需要手动增加内存屏障](../base/internal/memory_barrier.h)的特殊场景。[Linux内核中也有类似优化](https://lore.kernel.org/patchwork/patch/850075/)。*

- 核间通信（通常体现为缓存连贯性消息）成本。对于共享的原子变量被多核同时或先后使用，会导致各个核之间频繁通信，也是原子操作的伸缩性问题的主要所在。

核间通信的成本取决于物理拓扑，相同物理CPU节点内的通信成本通常明显低于不同物理CPU节点间的通信成本。比如对于多路Xeon，后者通常意味着需要通过[QPI](https://en.wikipedia.org/wiki/Intel_QuickPath_Interconnect)/[UPI](https://en.wikipedia.org/wiki/Intel_Ultra_Path_Interconnect)传输消息。

因此，对于原子变量，应当尽可能控制其共享范围。最好的情况是单个线程内访问（但是为什么不使用[`thread_local`](https://en.cppreference.com/w/cpp/keyword/thread_local)呢？），其次是同[调度组](scheduling-group.md)内（暗含“同物理CPU节点”内）共享，全局共享的原子变量应当尽量避免。

**对于大多数flare中的数据结构（如[IO时使用的非连续缓冲区](io.md)等），其（也即其内部包含的原子变量的）使用、共享范围通常被控制在一个调度内**。具体可参考“调度组”一节。

*关于共享状态的范围的讨论通常同样适用于其他类型的共享数据。*

### 相关信息

- 用于多线程分配（可能不连续）的ID时可以使用[`id_alloc::Next<Traits>()`](../base/id_alloc.h)避免在全局的`std::atomic<T>`上进行频繁的`fetch_add()`导致性能损耗。

## 共享指针、引用计数

不要企图用引用计数解决性能方面的问题（比如双缓冲保持缓冲区存活等等）。**引用计数不是性能问题的解决方案，引用计数本身就是性能问题。**

在可行的情况下，我们通常使用[侵入式的引用计数](../base/ref_ptr.h)。

我们的侵入式引用计数相对于[`std::shared_ptr`](https://en.cppreference.com/w/cpp/memory/shared_ptr)，性能方面有如下不同：

- 没有额外的内存分配：
  - 我们不支持自定义释放函数，因此不涉及到保存`Deleter`的内存开销（以及为对`Deleter`做[type erasure](https://en.wikipedia.org/wiki/Type_erasure)而引入的额外开销）
  - 因为引用计数是侵入到类本身的，因此不需要额外的“控制块”。（通常认为[`std::make_shared`](https://en.cppreference.com/w/cpp/memory/shared_ptr/make_shared)也可以做到这一点。）
- 不要求析构函数为虚：对于通过继承`RefCounted<T>`实现引用计数的类，我们不支持间接继承`RefCounted<U>`（而是要求必须直接继承`RefCounted<T>`）的情况。这样我们可以明确知道对象类型，进而不需要`T`本身析构函数为虚。避免了虚函数调用及相应的虚函数指针。
  - 但是如果析构函数确实定义成虚函数的话，其子类就不需要（但可以）单独定义`RefTraits<U>`了，我们会默认用其父类的`RefTraits<T>`。

另外需要指出的是，与通常的直觉不同，`std::atomic<T>`虽然“慢”，参见上文“原子变量”的讨论，如果不用于多线程之间同时修改引用计数，这并不会涉及到伸缩性问题。具体到我们的引用计数的场景，`RefCounted<T>`的实现中，初始化是没有原子RMW操作的成本的，如果不发生并发修改引用计数，那么从构造到析构只有析构时有一次不会对伸缩性造成影响的原子操作。

**但是除非真的有共享的必要，通常我们依然建议优先使用[`std::unique_ptr`](https://en.cppreference.com/w/cpp/memory/unique_ptr)管理动态分配的对象。**

## 锁

*不要*盲目的使用[`std::mutex`](https://en.cppreference.com/w/cpp/thread/mutex)等可能导致休眠的锁。

对于可能对系统整体性能造成影响的场景，如[对象池](object-pool.md)中的共享缓存，通常我们推荐在完成必要的状态分配之后（即保证临界区大小足够小且**耗时稳定**），通过[`Spinlock`](../base/spinlock.h)完成对共享数据的操作。

**这儿我们假定Flare的运行环境通常不会满载**。这意味着锁持有者通常不会被调度出去导致竞争方无意义的盲等（对于线上服务而言这通常是成立的）。

我们的`Spinlock`通过[ttas](https://en.wikipedia.org/wiki/Test_and_test-and-set)实现，其中fast-path（无竞争环境）会被内联，slow-path会导致函数调用。在特定负载（[fiber](../fiber/detail)、以及我们的对象池）中，我们的`Spinlock`实现实测性能略优于[`pthread_spin_lock`](http://man7.org/linux/man-pages/man3/pthread_spin_lock.3.html)。（`pthread_spin_lock`同样通过ttas实现，性能优势主要应当来源于内联等。）

取决于实现，`std::mutex`内部可能也会实现为短期的自旋+休眠，但是其在锁冲突较高且负载较高的情况下可能导致锁所有者被抢占导致其他线程阻塞，因此其**性能稳定性不可控**，特定负载下可能导致明显抖动。

如果无法避免且代码处在性能敏感的路径上（作为反例，[IO的写出](io.md)我们完全避免了加锁），我们推荐在*进行对比之后*选择合适的锁。

这儿我们只针对*Flare中明确的检验了临界区内的代码生成结果*的情况，**对于实际业务代码，使用`Spinlock`通常都是一个错误。**

## 链表

对于性能敏感的场景（尤其是多线程场景），使用链表时优先考虑我们的侵入式的链表[`internal::DoublyLinkedList`](../base/internal/doubly_linked_list.h)。

我们的侵入式链表内部不会出现内存分配释放，因此其性能表现*稳定*。对于多线程场景，这有助于控制临界区大小。

由于其临界区大小稳定，因此需要加锁时，通常推荐配合`Spinlock`使用（而不是`std::mutex`）。

## 线程局部存储

取决于链接器（[bfd](https://sourceware.org/binutils/docs/ld/BFD.html)、[gold](https://en.wikipedia.org/wiki/Gold_(linker))、...）的[优化能力](https://android.googlesource.com/platform/bionic/+/HEAD/docs/elf-tls.md#Current-Support-for-GD_LE-Relaxations-Across-Linkers)，不同程度优化后的[TLS变量的访问可能有不同的逻辑（及性能）](https://www.akkadia.org/drepper/tls.pdf)。

简单来说，ELF环境有如下几种[访问模型](https://gcc.gnu.org/onlinedocs/gcc/Code-Gen-Options.html#index-ftls-model)：

- Global Dynamic
  - 最通用的访问模型，支持[`dlopen`](https://linux.die.net/man/3/dlopen)等方式加载的动态库。
  - TLS访问需要通过调用[`__tls_get_addr`](https://code.woboq.org/userspace/glibc/sysdeps/x86_64/tls_get_addr.S.html)完成。
  - 我们的测试（x86-64）中这一模型一般会被优化至`initial-exec`，但不会优化为`local-exec`，这可能和指令长度有关（链接器优化TLS访问时通常需要填充[一或多字节的nop](https://stackoverflow.com/q/25545470)来保持指令长度不变，不同长度的指令可能会对链接器的[成本模型](https://en.wikipedia.org/wiki/Analysis_of_algorithms#Cost_models)有不同程度的影响）。
  - 时空（CPU开销及指令长度）性能较为一般。
- Local Dynamic
  - 同样需要`__tls_get_addr`。
  - 同一目标文件中不同的TLS变量通过偏移计算（包括访问目标文件中第一个变量也需要）而不需要每次都调用`__tls_get_addr`。
  - 这一模型对连续访问同一目标文件中多个TLS时相对Global Dynamic模型有少量性能提升，但由于每次都需要计算偏移，访问不同的目标文件时可能有性能退化。
  - flare中通常不考虑此种模型（除非链接器自动优化成此种模型。）。
- Initial Exec
  - 不支持`dlopen`加载的动态库，支持启动时加载的动态库。
  - 也因为上述限制，在创建第一个线程之前（或之时）就可以明确所有的Initial Exec（及下文所述的Local Exec）的TLS变量，直接将之分配在栈上，可直接通过相对栈底的偏移访问，无需`__tls_get_addr`。
  - 由于动态库中的符号可能涉及到[Symbol interposition](https://docs.oracle.com/cd/E19683-01/816-1386/chapter3-26/index.html)，因此这一访问模式在获取TLS相对于栈底的偏移时，需要访问[GOT](https://en.wikipedia.org/wiki/Global_Offset_Table)。
  - 我们的测试（x86-64）环境中，链接器通常会将这一模型优化至`local-exec`。
  - **对于性能敏感的TLS变量，flare通常会使用这一模型**。
- Local Exec
  - 只能用于可执行文件本身。
  - 无`tls_get_addr`成本。
  - 无访问GOT成本。
  - 四种模型中时空性能最好。

*部分性能对比可以参考[How fast is thread local variable access on Linux](https://stackoverflow.com/a/25481153)。这篇答案主要对比了普通变量、`local-exec`、`global-dynamic`之间的性能区别。取决于链接器是否提供了关闭优化的能力，简单的在同一个文件中使用四种不同的`gnu::tls_model`修饰做对比可能不能得到实际的对比结果（可参考反汇编结果判断是否被优化）。*

考虑到`local-exec`的应用范围过窄，因此一般不适宜直接将*flare中的TLS*（业务代码可视情况使用）标记为`local-exec`。同时，因为链接器通常可以将`initial-exec`优化至`local-exec`（但产出的指令长度可能不同，参见上文），我们通常会将使用频繁的TLS标记为`initial-exec`（如通过[C++属性](https://en.cppreference.com/w/cpp/language/attributes)`[[gnu::tls_model("initial-exec")]]`修饰TLS），以得到更好的性能。

*尽管Flare并不支持通过`dlopen`加载，但是对于特殊情况下需要避免使用`initial-exec`的场景，可以在编译时定义[`FLARE_USE_SLOW_TLS_MODEL`](../base/internal/annotation.h)宏。*

如对[`//flare/example/rpc:server`](../example/rpc/)（静态链接的二进制）中的[`fiber::fiber::detail::SetUpMasterFiberEntity()`](../fiber/detail/fiber_entity.cc)中下述代码：

```cpp
master_fiber = &master_fiber_impl;
current_fiber = master_fiber;
```

反汇编结果则如：

（不使用`gnu::tls_model`修饰，被自动优化至`initial-exec`。）

```asm
... <+105>: mov %fs:0x0,%rax
... <+114>: lea -0x138(%rax),%rax
... <+121>: mov %r12,(%rax)
... <+124>: mov %fs:0x0,%rax
... <+133>: lea -0x140(%rax),%rax
... <+140>: mov %r12,(%rax)
```

可以注意到上述指令中，访问TLS时需要访问GOT。

（使用`gnu::tls_model("initial-exec")`修饰，被优化为`local-exec`。）

```asm
... <+105>: mov $0xfffffffffffffec8,%rax
... <+112>: mov %r12,%fs:(%rax)
... <+116>: mov $0xfffffffffffffec0,%rax
... <+123>: mov %r12,%fs:(%rax)
```

可以注意到上述指令并未涉及到对GOT的访问，而是直接通过`fs`段访问了相关的TLS变量。

## 类型转换

我们参考[LLVM的实现](https://llvm.org/docs/HowToSetUpLLVMStyleRTTI.html)，实现了一套类似的[RTTI](https://en.wikipedia.org/wiki/Run-time_type_information)机制。

在我们的[性能测试](../base/casting_benchmark.cc)中，在Xeon Gold 6133上、简单的继承层次下（视继承层次的复杂度，这一开销可能不同。[`dynamic_cast`](https://en.cppreference.com/w/cpp/language/dynamic_cast)通常会随着继承层次复杂度增加而增大耗时）：

- 单次[`dynamic_cast`](https://en.cppreference.com/w/cpp/language/dynamic_cast)开销大约~19ns。
- 我们的[自制版](../base/casting.h)开销小于1ns。

*我们的测试环境中benchmark框架空跑大约耗时2ns，因此[`base/casting_benchmark.cc`](../base/casting_benchmark.cc)中的数据均比上述数据大~2ns。*

综上，C++内置的RTTI性能表现一般。对于性能敏感，或者在正常请求逻辑中处于常态的RTTI需求，我们推荐使用[我们自制的RTTI](../base/casting.h)。

关于我们自制版的RTTI的使用，可参考[rtti.md](rtti.md)。

---
[返回目录](README.md)
