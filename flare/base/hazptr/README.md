# Hazard pointers

对于读多写少的场景，Hazard pointer提供了相较读写锁而言更好的性能。其主要特点如下：

- （vs 读写锁）读方不会被写方阻塞；
- （vs RCU）被延迟释放的对象数量是确定的；
- （vs RCU）系统中不同位置的资源之间的释放不会互相影响，也因此可以容忍读方任意阻塞。

当然也需要注意到的是，Hazard pointer每次访问指针均会引入一个完整的内存屏障，特别是针对每个节点都受到Hazard pointer保护的链式结构，这可能会导致不必要的性能损耗。

*与之对比，RCU访问一整个对象（而不仅限于一个指针）通常不会或只引入确定个数个内存屏障。因此我们通常认为RCU保护整个数据结构，而Hazard pointer保护某个指针。*

另外需要注意的是，Hazard pointer可能导致对象实际释放被延迟，**这一延迟可能晚于所有读方均不再引用相关对象。**（我们后续可能会加入定时器定期清理过期对象来改善这一影响。）

## 接口

我们的参考[P1121R0](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1121r0.pdf)及[folly/Hazptr.h](https://github.com/facebook/folly/blob/master/folly/synchronization/Hazptr.h)实现了自己的Hazard pointer。

首先，为了使某个类型可以使用Hazard pointer保护，其需要从[`HazptrObject<T, D>`](hazptr_object.h)继承。`HazptrObject<...>`基类提供了部分内部所需的信息以便支持延迟释放。

```cpp
struct Buffer : public HazptrObject<Buffer> {
  // ... data
};
```

其次，我们定义了[`HazptrDomain`](hazptr_domain.h)用于将Hazard pointer关联到某个特定的作用域（即某个`HazptrDomain`对象），不同作用域之间是独立处理的互不干扰。除非显式指定，Hazard pointer相关的接口均默认使用“默认作用域”（通过`GetDefaultHazptrDomain()`获取）。我们通常**不推荐**用户自行指定作用域。

**需要注意的是，“默认”作用域我们做了额外优化，未经测试贸然使用独立的`HazptrDomain`可能会导致性能退化。**

最后，为了安全的访问可能随时被释放的指针，需要通过[`Hazptr`](hazptr.h)对相应的指针进行“保护”。通常，我们会通过某个共享的`std::atomic<T*>`实例来访问对象，为了避免访问过程中相应的指针被释放，可以通过`Hazptr::Keep(...)`保护。`Hazptr:Keep(...)`可以从一个`std::atomic<T*>`中读取指针并返回，并且保证返回的指针释放时机不早于下一次`Keep(...)`或`Hazptr`析构。

```cpp
std::atomic<Buffer*> shared_buffer = ...;

void Reader() {
  Hazptr hazptr;
  auto p = hazptr.Keep(&shared_buffer);
  // `p` won't be freed until `hazptr` itself is destroyed.
}
```

而对于写方，为了避免在读方访问过程中释放对象导致的use-after-free，应当通过`HazptrObject<...>`提供的`Retire()`方法来将对象交给框架并由之在合适的时机释放。

```cpp
void Writer() {
  auto new_buffer = std::make_unique<Buffer>();
  auto p = shared_buffer.exchange(new_buffer.release());  // Install new buffer.
  p->Retire();  // Queue old buffer for destruction.
}
```

*虽然Hazard pointer本身并不要求同时只能有一个写方在执行，多个线程同时更新也不会导致Hazard pointer自身产生错误（如泄露、崩溃等），但是通常的实际使用场景中，业务逻辑本身可能会需要对写方加锁避免构造过多不必要的数据。*

## 实现

内部实现而言：

- `Hazptr::Keep(...)`会将被保护的指针保存在自己的内部。
- `HazptrDomain`记录了所有存活的`Hazptr`的引用，因此可以检查所有`Hazptr`正在保护的指针。
- `HazptrObject::Retire(...)`在释放对象之前会检查所有`Hazptr`正在保护的指针，如果有`Hazptr`正在引用这个指针，则暂时不进行实际的释放，等到下次`Retire(...)`时再检查一次。

因此同时最多可能存在`Hazptr`实例数个对象同时存活（每个`Hazptr`都引用了不同版本的对象），考虑到一般`Hazptr`的实例数和线程数呈线性关系，因此理论上同时存活的对象个数也为O(线程数)。不过实际场景中一般同时存活的对象个数并不会很多。

目前我们的实现仅对读方进行了优化（[每次`Hazptr`构造+`Keep(...)`的开销~10ns](../hazptr_benchmark.cc)），并未对写方（主要是检查所有`Hazptr`等逻辑）进行优化。考虑到Hazard pointer本身也只用于读多写少的场景，因此我们暂时不认为有对写方进行优化的必要。
