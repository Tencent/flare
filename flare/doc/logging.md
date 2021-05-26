# 日志

我们对[glog](https://github.com/google/glog)做了二次包装，提供了一些额外的宏方便使用，并改善了性能。

我们提供的宏功能与glog类似，输出格式相同。但是代码中不再使用流式输出，而使用[C++20的格式化库](https://en.cppreference.com/w/cpp/utility/format)的方式输出，如：

```cpp
FLARE_LOG_INFO("The Answer to the Ultimate Question of Life, the Universe, and Everything is {}.", 42);
```

这有助于改善代码中的日志格式串的可读性，并有助于我们优化代码生成（见后文）。

## 接口

具体我们提供的宏列表可以参见[源代码](../base/logging.h)。

除了常用的`FLARE_VLOG` / `FLARE_(D|P)LOG_XXX(_IF)(_EVERY_N)` / `FLARE_(D|P)CHECK(_XX)`外，我们还提供了如下一些宏：

- `FLARE_LOG_XXX(_IF)_EVERY_SECOND`：通过这个宏输出的日志每秒至多输出一条。这有助于避免某些场景下大量出错或警告日志而拖慢程序运行进而引发更大的问题。（这一宏受[brpc](https://github.com/apache/incubator-brpc/blob/master/src/butil/logging.h)启发，谨此致谢。）
- `FLARE_LOG_XXX_ONCE`：通过这个宏输出的日志只会输出一次。

除部分宏额外接受的参数（如`FLARE_CHECK`第一个参数接受`expr`、`FLARE_LOG_INFO_EVERY_N`第一个参数接受`n`等）之外，所有宏接受的参数均为一个格式串+0或多个格式化参数。格式串的定义参考[`std::format`](https://en.cppreference.com/w/cpp/utility/format)。

如：

```cpp
int x = 1, y = 2;
std::string reason = "Nobody knowns.";

// Prints "x (1) does not equal to y (2). The reason is that [Nobody knowns.].".
FLARE_CHECK_EQ(x, y, "x ({}) does not equal to y ({}). The reason is that [{}].",
    x, y, reason);  // No need to write `reason.c_str()`. C++ types are recognized.
```

*受限于编译器附带的标准库对C++20的支持，我们实际实现中使用[libfmt](https://github.com/fmtlib/fmt)实现。libfmt也是C++20的格式化库的提案原型。*

### 自定义前缀

我们提供了一些方法来帮助服务实现方在某种环境（见下）增加*所有*通过`FLARE_XXX`输出的日志的自定义的前缀，这某些情况下有助于调试（线下或线上）。

我们提供了如下几个方法：

- [`flare::AddLoggingItemToRpc`、`flare::AddLoggingTagToRpc`](../rpc/logging.h)：这一方法可以增加一个（或多次调用的时候，可以增加多个）前缀至所有后续*在这一RPC上下文中*输出的日志。前者用于直接增加一个字符串，后者用于增加KV结构数据。[如](../rpc/logging_test.cc)：

  ```cpp
  void EchoServiceImpl::Echo(const EchoRequest& request, EchoResponse* response,
                             flare::RpcServerController* ctlr) override {
    AddLoggingTagToRpc("crash_id", request.body());  // String.
    FLARE_LOG_INFO("crashing.");
    FLARE_LOG_WARNING("crashing..");
    FLARE_LOG_ERROR("crashing...");

    AddLoggingItemToRpc(Format("crash_id 2: {}4", request.body()));  // KV-pair.
    FLARE_LOG_INFO("crashing.");
    FLARE_LOG_WARNING("crashing..");
    FLARE_LOG_ERROR("crashing...");
  }
  ```

  则会输出：

  ```text
  I0101 00:00:00.000000 1 flare/rpc/logging_test.cc:35] [crash_id: body123] crashing.
  W0101 00:00:00.000000 1 flare/rpc/logging_test.cc:36] [crash_id: body123] crashing..
  E0101 00:00:00.000000 1 flare/rpc/logging_test.cc:37] [crash_id: body123] crashing...
  I0101 00:00:00.000000 1 flare/rpc/logging_test.cc:40] [crash_id: body123] [crash_id 2: body1234] crashing.
  W0101 00:00:00.000000 1 flare/rpc/logging_test.cc:41] [crash_id: body123] [crash_id 2: body1234] crashing..
  E0101 00:00:00.000000 1 flare/rpc/logging_test.cc:42] [crash_id: body123] [crash_id 2: body1234] crashing...
  ```

  可以看到，所有通过`FLARE_XXX`在这一RPC上下文中输出的日志均带有`crash_id: body123`的前缀，在第二次`AddLoggingItemToRpc`之后还有`crash_id 2: body1234`前缀。

  另外，上述代码未体现的是：即便对于并行代码（通过[`flare::Async`](../fiber/async.h)发等起的）的场景，只要能够正确传递执行上下文（此处的要求同[分布式追踪](tracing.md)），这儿的日志前缀就可以正确的保留、传递。

- [`flare::fiber::AddLoggingItemToFiber`、`flare::fiber::AddLoggingTagToFiber`](../fiber/logging.h)：这一方法允许增加一个针对当前[fiber](fiber.md)的日志前缀。通常而言这个方法较为底层，我们不推荐服务实现方直接使用。
- [`flare::fiber::AddLoggingItemToExecution`、`flare::fiber::AddLoggingTagToExecution`](../fiber/logging.h)：这一方法允许增加一个针对当前[执行上下文](../fiber/execution_context.h)的日志前缀。这一方法较为底层通常不推荐服务实现方使用。

## 性能

在可行的情况下应当尽量使用我们提供的[`FLARE_CHECK_XX` / `FLARE_LOG_XXX`](../base/logging.h)宏。

通常情况下我们的日志输出与glog相同，但是我们所使用的glog版本（可能更新的版本存在类似问题）生成的代码质量并不高。这里的问题主要为如下几点：

- `CHECK_OP`系列缺少合适的`__builtin_expect(...)`。严格来说，glog的确通过`CheckOpString::operator bool()`来尝试向GCC提供“分支不成立”的暗示。但是似乎是由于这种方式过于隐晦，在我们的观察中，GCC 8.2并不能正确的识别这种暗示。因此最终生成的代码中这些断言失败的处理逻辑会穿插在正常业务代码中。尽管这些暗示通常不影响现代CPU的分支预测逻辑，但是正常代码过于分散确实会对L1i、iTLB等的命中率产生负面影响。当断言失败时需要输出信息时（因此有额外构造错误信息的代码），这种代码生成结果的影响尤为明显。

- `CHECK_OP`生成的代码量庞大，包括但不限于`google::LogMessageXxx`的构造，`operator<<`导致的`std::stream`等等。这对于一些本可以被内联的小方法而言，一两个`CHECK_OP`可能就会导致其由于过于庞大而不能被内联，因此导致性能损失。

我们的`FLARE_CHECK_OP`系列采取了以下一些措施来解决这些问题：

- `FLARE_CHECK_OP`直接展开为`if (FLARE_UNLIKELY(...)) { ... }`，这种写法是编译器最擅长识别的暗示，因此通常都可以正常处理。

- 在减少生成代码量方面，我们通过构造立即执行的lambda输出日志。我们会并通过`__attribute__((noinline, cold))`（[这儿不能使用C++的`[[...]]`形式的attribute](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=89640)）修饰这一lambda。这样lambda会被编译为放在“冷”代码段的独立函数。并且由于lambda不需要对外暴露，因此生成代码的时候不需要使用标准的calling convention，因此相比于通常的调用方法，这儿生成的代码也往往不需要生成传递参数的逻辑，非常简洁（通常只有一条`jmp`指令+一条`call`指令）。

  *由于Clang10（及之前版本）未实现[P1381R1](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p1381r1.html)，为了保证打印日志时可以正常访问[Structured Binding](https://en.cppreference.com/w/cpp/language/structured_binding)引入的标识符，在Clang中我们不会使用这个优化。*

`FLARE_LOG_XXX_IF`对warning及以上级别增加了`__builtin_expect(...)`来帮助编译器生成更好的代码排布。

如果需要对比，也可以通过观察[`logging_benchmark.cc`](../base/logging_benchmark.cc)的编译结果来实现。需要注意的是，由于这个benchmark所测试的代码足够短且分支预测几乎不会出错，因此这儿的性能结果没有参考价值，其的存在主要是用于方便对比代码生成结果。

此处给出`CHECK_LE`和`FLARE_CHECK_LE`的对比（源代代码参见`logging_benchmark.cc`）。

```asm
# CHECK_LE(x, y);
# CHECK_LE(x, y) << "Some sophisticated log [" << x << ", " << y << "].";

# Code generated for benchmark framework not shown ...
0x00000000004108d9 <+57>:    lea    0x155528(%rip),%rbx        # 0x565e08 <_ZN5flare1xE>
0x00000000004108e0 <+64>:    mov    %rax,(%r12)
0x00000000004108e4 <+68>:    lea    0x155519(%rip),%rax        # 0x565e04 <_ZN5flare1yE>
0x00000000004108eb <+75>:    mov    (%rax),%edx
0x00000000004108ed <+77>:    mov    (%rbx),%eax
0x00000000004108ef <+79>:    mov    %edx,-0x40(%rbp)
0x00000000004108f2 <+82>:    mov    %eax,-0x48(%rbp)
0x00000000004108f5 <+85>:    cmp    %eax,%edx
0x00000000004108f7 <+87>:    jge    0x410914 <flare::Benchmark_CheckOp(benchmark::State&)+116>
0x00000000004108f9 <+89>:    mov    %r15,%rdx
0x00000000004108fc <+92>:    mov    %r13,%rsi
0x00000000004108ff <+95>:    mov    %r14,%rdi
0x0000000000410902 <+98>:    callq  0x41cc10 <google::MakeCheckOpString<int, int>(int const&, int const&, char const*)>
0x0000000000410907 <+103>:   mov    %rax,-0x48(%rbp)
0x000000000041090b <+107>:   test   %rax,%rax
0x000000000041090e <+110>:   jne    0x410a0e <flare::Benchmark_CheckOp(benchmark::State&)+366>
0x0000000000410914 <+116>:   lea    0x1554e9(%rip),%rax        # 0x565e04 <_ZN5flare1yE>
0x000000000041091b <+123>:   mov    (%rax),%edx
0x000000000041091d <+125>:   mov    (%rbx),%eax
0x000000000041091f <+127>:   mov    %edx,-0x40(%rbp)
0x0000000000410922 <+130>:   mov    %eax,-0x48(%rbp)
0x0000000000410925 <+133>:   cmp    %eax,%edx
0x0000000000410927 <+135>:   jge    0x4108c8 <flare::Benchmark_CheckOp(benchmark::State&)+40>
0x0000000000410929 <+137>:   mov    %r15,%rdx
0x000000000041092c <+140>:   mov    %r13,%rsi
0x000000000041092f <+143>:   mov    %r14,%rdi
0x0000000000410932 <+146>:   callq  0x41cc10 <google::MakeCheckOpString<int, int>(int const&, int const&, char const*)>
0x0000000000410937 <+151>:   mov    %rax,-0x48(%rbp)
0x000000000041093b <+155>:   test   %rax,%rax
0x000000000041093e <+158>:   je     0x4108c8 <flare::Benchmark_CheckOp(benchmark::State&)+40>
0x0000000000410940 <+160>:   mov    $0x40,%edx
0x0000000000410945 <+165>:   lea    0x1012b4(%rip),%rsi        # 0x511c00
0x000000000041094c <+172>:   mov    %r14,%rcx
0x000000000041094f <+175>:   mov    %r13,%rdi
0x0000000000410952 <+178>:   callq  0x431260 <google::LogMessageFatal::LogMessageFatal(char const*, int, google::CheckOpString const&)>
0x0000000000410957 <+183>:   mov    -0x38(%rbp),%rax
0x000000000041095b <+187>:   lea    0xff0f3(%rip),%rdi        # 0x50fa55
0x0000000000410962 <+194>:   mov    0x20(%rax),%r12
0x0000000000410966 <+198>:   callq  0x410b90 <std::char_traits<char>::length(char const*)>
0x000000000041096b <+203>:   lea    0xff0e3(%rip),%rsi        # 0x50fa55
0x0000000000410972 <+210>:   mov    %rax,%rdx
0x0000000000410975 <+213>:   mov    %r12,%rdi
0x0000000000410978 <+216>:   callq  0x4ed060 <_ZSt16__ostream_insertIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_PKS3_l>
0x000000000041097d <+221>:   mov    (%rbx),%esi
0x000000000041097f <+223>:   mov    %r12,%rdi
0x0000000000410982 <+226>:   callq  0x4ed780 <_ZNSolsEi>
0x0000000000410987 <+231>:   lea    0xff155(%rip),%rdi        # 0x50fae3
0x000000000041098e <+238>:   mov    %rax,%rbx
0x0000000000410991 <+241>:   callq  0x410b90 <std::char_traits<char>::length(char const*)>
0x0000000000410996 <+246>:   lea    0xff146(%rip),%rsi        # 0x50fae3
0x000000000041099d <+253>:   mov    %rbx,%rdi
0x00000000004109a0 <+256>:   mov    %rax,%rdx
0x00000000004109a3 <+259>:   callq  0x4ed060 <_ZSt16__ostream_insertIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_PKS3_l>
0x00000000004109a8 <+264>:   lea    0x155455(%rip),%rax        # 0x565e04 <_ZN5flare1yE>
0x00000000004109af <+271>:   mov    %rbx,%rdi
0x00000000004109b2 <+274>:   mov    (%rax),%esi
0x00000000004109b4 <+276>:   callq  0x4ed780 <_ZNSolsEi>
0x00000000004109b9 <+281>:   lea    0xff0ae(%rip),%rsi        # 0x50fa6e
0x00000000004109c0 <+288>:   mov    %rax,%rdi
0x00000000004109c3 <+291>:   callq  0x4ed470 <_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc>
0x00000000004109c8 <+296>:   mov    %r13,%rdi
0x00000000004109cb <+299>:   callq  0x431270 <google::LogMessageFatal::~LogMessageFatal()>
# Code generated for benchmark framework not shown ...
0x0000000000410a0e <+366>:   mov    %r13,%rdi
0x0000000000410a11 <+369>:   mov    %r14,%rcx
0x0000000000410a14 <+372>:   mov    $0x3f,%edx
0x0000000000410a19 <+377>:   lea    0x1011e0(%rip),%rsi        # 0x511c00
0x0000000000410a20 <+384>:   callq  0x431260 <google::LogMessageFatal::LogMessageFatal(char const*, int, google::CheckOpString const&)>
0x0000000000410a25 <+389>:   mov    %r13,%rdi
0x0000000000410a28 <+392>:   callq  0x431270 <google::LogMessageFatal::~LogMessageFatal()>
0x0000000000410a2d <+397>:   jmpq   0x4079ad <_ZN5flare17Benchmark_CheckOpERN9benchmark5StateE.cold.298>

# .cold section
0x00000000004079ad <-36595>: mov    %r13,%rdi
0x00000000004079b0 <-36592>: callq  0x431270 <google::LogMessageFatal::~LogMessageFatal()>
```

```asm
# FLARE_CHECK_LE(x, y);
# FLARE_CHECK_LE(x, y, "Some sophisticated log [{}, {}].", x, y);

# Code generated for benchmark framework not shown ...
0x0000000000410838 <+24>:    lea    0x1555c9(%rip),%rdx        # 0x565e08 <_ZN5flare1xE>
0x000000000041083f <+31>:    mov    %rax,(%rbx)
0x0000000000410842 <+34>:    lea    0x1555bb(%rip),%rax        # 0x565e04 <_ZN5flare1yE>
0x0000000000410849 <+41>:    mov    (%rdx),%esi
0x000000000041084b <+43>:    mov    (%rax),%ecx
0x000000000041084d <+45>:    cmp    %ecx,%esi
0x000000000041084f <+47>:    jg     0x4079a3 <_ZN5flare22Benchmark_FlareCheckOpERN9benchmark5StateE.cold.297>
0x0000000000410855 <+53>:    mov    (%rdx),%edx
0x0000000000410857 <+55>:    mov    (%rax),%eax
0x0000000000410859 <+57>:    cmp    %eax,%edx
0x000000000041085b <+59>:    jg     0x4079a8 <flare::Benchmark_FlareCheckOp(benchmark::State&)-36472>
# Code generated for benchmark framework not shown ...

# .cold section
0x00000000004079a3 <-36477>: callq  0x40787a <flare::<lambda()>::operator()(void) const>
0x00000000004079a8 <-36472>: callq  0x4078ec <flare::<lambda()>::operator()(void) const>
```

---
[返回目录](README.md)
