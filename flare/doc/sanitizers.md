# Sanitizers

新版本的GCC/Clang内置了[sanitizers](https://github.com/google/sanitizers)，可以用于发现代码中的bug。

flare对sanitizers提供了*有限的*支持。

## UBSan / LSan

UBSan / LSan直接支持fiber运行环境，因此flare默认支持这两种sanitizers。

为使用UBSan / LSan，编译*及链接*时增加参数（通常需要修改`BLADE_ROOT`）`-fsanitize=undefined`、`-fsanitize=leak`即可。

## ASan

为了支持ASan，需要在创建、切换fiber时通知ASan的运行时。[flare的fiber实现中](../fiber/detail)已经增加了必要的[annotations](../base/internal/annotation.h)，在足够新的编译器中（如GCC8+）默认支持ASan。

为使用ASan，编译及链接时增加参数`-fsanitize=address`即可。

### 常见问题

#### 编译器过老

如果编译器携带的ASan版本未提供对fiber的支持，运行编译产出会立刻输出如下错误并崩溃：

> Your compiler is too old to use ASan with fibers.

#### 洋葱系统干扰

取决于运行时环境，对于公司内环境由于洋葱系统会通过`/etc/ld.so.preload`提前加载，因此会导致ASan的运行时初始化时机过晚，输出类似如下错误：

> ==10000==ASan runtime does not come first in initial library list; you should either link runtime to your application or manually preload it with LD_PRELOAD.

可通过`LD_PRELOAD=/path/to/gcc/lib64/libasan.so /path/to/executable`的方式运行编译产出来解决这一问题。

### 实现细节

未显式支持ASan的fiber库通常在ASan环境下会导致ASan崩溃。

为了支持fiber环境，[ASan加入了`sanitizer_start_switch_fiber`、`sanitizer_finish_switch_fiber`两个方法](https://reviews.llvm.org/D20913)。

关于这两个方法，我们在[`flare/base/internal/annotation.h`](../base/internal/annotation.h)中以我们对文档及ASan的代码的理解进行了注解。

简单来说，在创建、切换、销毁时，flare均会通过这两个方法通知ASan运行时，以使其能够正确的维护其内部的数据结构（主要是[fake stack，这一概念在use-after-return的wiki中有所提及](https://github.com/google/sanitizers/wiki/AddressSanitizerUseAfterReturn)）。

另外需要注意的是，对于*非*ASan运行时分配的内存，或（对于使用对象池重用fiber的栈的情况）重用旧的内存块时，需要通知ASan这块内存应当被视为“全新”的，这一行为可通过`__asan_unpoison_memory_region`完成。我们对这一方法的接口及其行为理解同样可以参见[`flare/base/internal/annotation.h`](../base/internal/annotation.h)。

flare通过[对象池的钩子](object-pool.md)，在栈被重新使用时会通过`__asan_unpoison_memory_region`“刷新”栈所使用的内存。部分支持了ASan的fiber库会选择在栈被回收时即进行unpoisoning，这可能导致某些use-after-free无法被正确的检测到。

*理论上来说，更好的做法是在ASan环境下从不复用栈，这样可以避免栈被复用导致的某些use-after-free无法被正确的检测。我们将会视人力对此进行改进。*

## TSan

为了支持TSan，同样需要在创建、切换fiber时通知TSan的运行时。flare的fiber实现中增加了必要的[annotations来通知TSan关于fiber的创建、切换及销毁](../base/internal/annotation.h)。

*TSan自身对Fiber的支持较晚，需要GCC10+ / Clang9+后的版本才能支持Fiber环境的TSan。*

为使用TSan，编译及链接时增加参数`-fsanitize=thread`即可。

### 常见问题

#### 编译器过老

如果编译器携带的TSan版本未提供对fiber的支持，运行编译产出会立刻输出如下错误并崩溃：

> Your compiler is too old to use TSan with fibers.

### 实现细节

未显式支持TSan的fiber库通常在TSan环境下会导致TSan崩溃。

于ASan类似，TSan同样需要在fiber创建、切换、销毁时更新其内部的数据结构（主要是fake stack）。

TSan使用不同于ASan的接口来通知TSan的运行时：

- `__tsan_create_fiber`
- `__tsan_destroy_fiber`
- `__tsan_switch_to_fiber`

我们对这些接口及其内部实现的理解可以参见[`flare/base/internal/annotation.h`](../base/internal/annotation.h)中的注释。这一文件亦包含了部分TSan提供的其他针对fiber的接口（如用于改善调试体验的`__tsan_set_fiber_name`）。

### 其他问题

正如[brpc的文档中针对bthread环境下的TLS的描述](https://github.com/apache/incubator-brpc/blob/master/docs/cn/thread_local.md)，对于发生用户态调度的环境，如果同一个方法（某些情况下可能可以跨越多个方法，如发生inline的情况）多次访问TLS之间如果发生上下文切换，如果切换后所执行的pthread发生了变化，可能导致实际访问到了之前线程的TLS，这通常是一个错误。

但是单纯的屏蔽`__attribute((const))`在TSan环境下并不能完全的解决这个问题。根据我们观察，这跟启用TSan后的代码生成有关（多次访问TLS依然会进行优化）。

严格来说，这儿编译器确实有进行（会导致用户态线程出错的）优化的自由，更多讨论可以参考[GCC Bugzilla #26461](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=26461)。

目前我们注意到的问题主要集中在访问[`flare::fiber::detail::current_fiber`](../fiber/detail/fiber_entity.h)。针对这一问题我们在TSan环境下将访问这一变量的方法进行了条件编译，避免GCC进行[CSE](https://en.wikipedia.org/wiki/Common_subexpression_elimination)。

但是需要注意的是，**这一修改只解决了TSan报出的（目前来说）false-positive**，在不启用TSan的环境中，**依然有发生CSE导致问题的风险**。

由于实践中我们代码实际大量使用了TLS，如果我们更新了编译器之后真的进行了类似的优化，程序起来后很快就会崩溃，可以**很快发现问题**。考虑到我们更新编译器相对不频繁且出问题时容易暴露，因此我们暂时不对这一问题做过多考虑。

我们仍然需要一个符合C++标准或受编译器明确支持的解决方案。

---
[返回目录](README.md)
