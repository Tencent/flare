# 缓冲区

受[butil::IOBuf](https://github.com/apache/incubator-brpc/blob/master/docs/cn/iobuf.md)的启发，我们也实现了一套非连续的缓冲区[`NoncontiguousBuffer`](../base/buffer.h)。

这套缓冲区的实现也是通过将内存分成多个块，并使用给缓冲区块增加引用计数、并用链表将区块串联起来得到最终的缓冲区。

这样复制缓冲区的成本只在于更新引用计数及复制链表，而不需要复制实际的缓冲区的内容，避免反复复制缓冲区导致的性能开销。

## 缓冲区块大小

默认配置下每个缓冲区块的大小约4KB，这应当可以在内存开销和性能之间区的一个相对合理的平衡。

部分业务可能通常处理的包都较大（100K乃至更大），这时候默认的4K包大小导致的如引用计数等成本可能相对显著。此时**可以通过`--flare_buffer_block_size={4K|64K|1M}`选项修改缓冲区块大小**。

*我们的测试中同机压测，客户端、服务端均8线程时，回显128K的附件包，64K相对于4K缓冲区块有大约20%的吞吐收益。*

通常来说，因为业务的包大小（类别，非字节数）不会随着业务迭代反复波动，而是相对稳定的。因此，如果需要修改`flare_buffer_block_size`，我们实际开发中会通过[`FLARE_OVERRIDE_FLAG`](gflags.md)在代码中直接设置：

```cpp
FLARE_OVERRIDE_FLAG(flare_buffer_block_size, "64K");  // <- !!!

int main(int argc, char** argv) {
  // ...
}
```

**将这一参数设置的太大时，如果缓冲区块的实际利用率太低，可能会大大增加进程的内存占用（见后文）**。

### 重新打包缓冲区

如果使用非默认设置的缓冲区块（如修改为64K），取决于实际业务场景，可能会因为区块的实际利用率太低，而导致64K的区块中大量的内存被浪费。这会导致进程的内存占用远远超出预期。

取决于具体场景，如果需要长期保存缓冲区，可以考虑将缓冲区重新打包（如通过`std::string`保存）：

```cpp
flare::NoncontiguousBuffer RepackNoncontiguousBuffer(
    const flare::NoncontiguousBuffer& buffer) {
  auto flatten = flare::FlattenSlow(buffer);
  flare::NoncontiguousBuffer result;
  result.Append(flare::MakeForeignBuffer(std::move(flatten)));
  return result;
}
```

## 和外界缓冲区整合

另外，考虑到实际的业务场景，我们还提供了[`PolymorphicBuffer`](../base/buffer/polymorphic_buffer.h)以便接受多种不同来源的缓冲区。

这些缓冲区可以用来构造[RPC请求的附件](protocol/protocol-buffers.md)、[HTTP的响应](intro-http.md)及其他Flare中接收`NoncontiguousBuffer`的地方的数据。除非特别声明，Flare通常并不关注`NoncontiguousBuffer`中的数据的来源及大小。

**如果缓冲区块足够小，通过一次或多次`memcpy`将之合并成一个大缓冲区块往往比“零拷贝”构造的一系列小缓冲区块性能表现更好。仅当外界的缓冲区足够大时才应当考虑下述方法。**

目前我们支持如下来源的（外界）缓冲区：

- 引用外界的缓冲区。

  这通常通过[`MakeReferencingBuffer(...)`](../base/buffer.h)实现。

  这个方法接受一个[`std::string_view`](https://en.cppreference.com/w/cpp/string/basic_string_view)，指向需要引用的缓冲区。

  另外，这个方法还（可选的）接受一个回调，当缓冲区对象（`PolymorphicBuffer`）析构时回调用这个回调。这个回调可以用来通知用户“缓冲区可以被释放”。

- 由用户构造好的`std::string` / `std::vector<char>` / `std::vector<std::byte>`。

  这可以通过[`MakeForeignBuffer`](../base/buffer.h)实现。

  这个方法会将参数中的容器`std::move`到返回的对象内部。

  由于这几种容器的`std::move`操作相对快捷（不涉及内存拷贝），因此相对于`CreateBufferSlow`而言，可以改善性能。

### 使用

具体使用可以参考我们的[单元测试](../base/buffer_test.cc)：

```cpp
NoncontiguousBufferBuilder nbb;
nbb.Append(MakeReferencingBuffer("abcdefg"sv));
auto result = nbb.DestructiveGet();
```

```cpp
NoncontiguousBufferBuilder nbb;
nbb.Append(MakeReferencingBuffer(std::string_view(some_mmaped_ptr, size),
                                 [] { /* Now buffer can be `munmap`-ed*/ }));
auto result = nbb.DestructiveGet();
```

```cpp
std::vector<char> data{'a', 'b', 'c', 'd', 'e', 'f', 'g'};
NoncontiguousBufferBuilder nbb;
nbb.Append(MakeForeignBuffer(std::move(data)));
auto result = nbb.DestructiveGet()));
```
