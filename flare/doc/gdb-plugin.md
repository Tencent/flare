# GDB插件

我们提供了GDB插件以改善fiber环境中的调试体验。我们的实现支持**枚举正在运行的进程中的fiber及枚举coredump中的fiber**。

*GDB版本要求至少为7.6（CentOS7 / tliunx2自带）或更高，我们推荐使用GDB 8或更新的版本.*

为使用我们的GDB插件，启动GDB后需要执行如下命令：

```shell
source /path/to/flare/fiber/gdb-plugin.py
```

对于开发环境，也可以将上述命令加入[.gdtinit](https://sourceware.org/gdb/current/onlinedocs/gdb/gdbinit-man.html)中，这样就不需要每次执行了。

*脚本稳定后我们可能会[通过`.debug_gdb_scripts`节默认加载](https://sourceware.org/gdb/current/onlinedocs/gdb/Python-Auto_002dloading.html)我们的GDB插件。*

之后即可以在GDB中通过命令下述命令来枚举运行中的fibers了：

- `list-fibers`：枚举进程中的fibers。
- `list-fibers-compact`：同`list-fibers`，但是在输出之前会合并call-stack相同的fibers，更便于观察。

对于线上环境，可以通过命令`gdb --pid <进程PID> --eval-command='source gdb-plugin.py' --eval-command='set pagination off' --eval-command='list-fibers' --batch`尽可能减少调试对运行中服务的影响（**即便如此下述命令仍然会引起秒级服务挂起**）。

*执行之前需要在当前目录准备好[`gdb-plugin.py`](../tools/gdb-plugin.py)。*

`list-fibers`输出示例（前两个fiber调用栈相同，未合并，分别输出）：

```gdb
(gdb) list-fibers
Fiber #524446:
#0 0x00007ffff7308e65 flare::fiber::detail::FiberEntity::Resume() + 117 [flare/fiber/detail/fiber_entity.cc:186]
#1 0x00007ffff730909a flare::fiber::detail::FiberEntity::ResumeOn(gdt::Function<void ()>) + 218 [flare/fiber/detail/fiber_entity.cc:204]
#2 0x00007ffff731ecb0 flare::fiber::detail::SchedulingGroup::Halt(flare::fiber::detail::FiberEntity*, std::unique_lock<flare::Spinlock>&&) + 176 [./common/base/function.h:87]
#3 0x00007ffff732875f flare::fiber::detail::ConditionVariable::wait_until(std::unique_lock<flare::fiber::detail::Mutex>&, std::chrono::time_point<std::chrono::_V2::steady_clock, std::chrono::duration<long, std::ratio<1l, 1000000000l> > >) + 639 [flare/fiber/detail/waitable.cc:275]
#4 0x00007ffff7328aab flare::fiber::detail::ConditionVariable::wait(std::unique_lock<flare::fiber::detail::Mutex>&) + 27 [flare/fiber/detail/waitable.cc:251]
#5 0x00007ffff7654544 flare::fiber::WorkQueue::WorkerProc() + 1140 [/usr/local/tools/gcc/include/c++/8.2.0/bits/stl_deque.h:1367]
#6 0x00007ffff7654684 gdt::Function<void ()>::ErasedCopySmall<flare::fiber::WorkQueue::WorkQueue()::{lambda()#1}>(void*, flare::fiber::WorkQueue::WorkQueue()::{lambda()#1}&&)::{lambda(gdt::Function<void ()>::TypeOps const*)#1}::_FUN(gdt::Function<void ()>::TypeOps const*) + 20 [flare/fiber/work_queue.cc:15]
#7 0x00007ffff7309357 flare::fiber::detail::(anonymous namespace)::FiberProc(void*) + 215 [./common/base/function.h:87]

Fiber #131076:
#0 0x00007fffef423933 epoll_wait + 51 [(Unknown)]
#1 0x00007ffff74ce928 flare::EventLoop::WaitAndRunEvents(std::chrono::duration<long, std::ratio<1l, 1000l> >) + 56 [/usr/local/tools/gcc/include/c++/8.2.0/chrono:510]
#2 0x00007ffff74cedad flare::EventLoop::Run() + 205 [flare/io/event_loop.cc:197]
#3 0x00007ffff74cf130 gdt::Function<void ()>::ErasedCopySmall<flare::StartAllEventLoops()::{lambda()#1}&>(void*, flare::StartAllEventLoops()::{lambda()#1}&)::{lambda(gdt::Function<void ()>::TypeOps const*)#1}::_FUN(gdt::Function<void ()>::TypeOps const*) + 112 [/usr/local/tools/gcc/include/c++/8.2.0/bits/unique_ptr.h:342]
#4 0x00007ffff7309357 flare::fiber::detail::(anonymous namespace)::FiberProc(void*) + 215 [./common/base/function.h:87]

(...)

Fiber #1:
#0 0x00007fffefba3945 pthread_cond_wait@@GLIBC_2.3.2 + 197 [(Unknown)]
#1 0x00007ffff1a8372b gdt::PollThread::Stop() + 171 [common/system/io_frame/poll_thread.cc:53]
#2 0x00007ffff22e3673 gdt::HttpServer::Stop() + 339 [common/net/http/server/http_server.cc:347]
#3 0x0000000000427dd2 flare::protobuf::ProtoOverHttpProtocolStreamingRpc_ServerSideGdt_Test::TestBody() + 1906 [flare/rpc/protobuf/proto_over_http_protocol_streaming_rpc_test.cc:304]
#4 0x00007ffff07eb8fe void testing::internal::HandleExceptionsInMethodIfSupported<testing::Test, void>(testing::Test*, void (testing::Test::*)(), char const*) + 78 [thirdparty/googletest-1.8.1/googletest/src/gtest.cc:2424]
#5 0x00007ffff07d2dcf testing::Test::Run() + 287 [thirdparty/googletest-1.8.1/googletest/src/gtest.cc:2517]
#6 0x00007ffff07d2f70 testing::TestInfo::Run() + 400 [thirdparty/googletest-1.8.1/googletest/src/gtest.cc:2693]
#7 0x00007ffff07d3085 testing::TestCase::Run() + 261 [thirdparty/googletest-1.8.1/googletest/src/gtest.cc:2811]
#8 0x00007ffff07dfe64 testing::internal::UnitTestImpl::RunAllTests() + 1764 [/usr/local/tools/gcc/include/c++/8.2.0/bits/stl_vector.h:930]
#9 0x00007ffff07eb0ee bool testing::internal::HandleExceptionsInMethodIfSupported<testing::internal::UnitTestImpl, bool>(testing::internal::UnitTestImpl*, bool (testing::internal::UnitTestImpl::*)(), char const*) + 78 [thirdparty/googletest-1.8.1/googletest/src/gtest.cc:2424]
#10 0x00007ffff07d2358 testing::UnitTest::Run() + 168 [./thirdparty/googletest-1.8.1/googletest/include/googletest/gtest/gtest.h:1340]
#11 0x00007ffff7e5a3b1 gdt::Function<int (int, char**)>::ErasedCopySmall<flare::testing::InitAndRunAllTests(int*, char**)::{lambda(auto:1, auto:2)#1}>(void*, flare::testing::InitAndRunAllTests(int*, char**)::{lambda(auto:1, auto:2)#1}&&)::{lambda(gdt::Function<int (int, char**)>::TypeOps const*, int&&, char**&&)#1}::_FUN(gdt::Function<int (int, char**)>::TypeOps const*, flare::testing::InitAndRunAllTests(int*, char**)::{lambda(auto:1, auto:2)#1}&&, int&&) + 33 [./thirdparty/googletest-1.8.1/googletest/include/googletest/gtest/gtest.h:2341]
#12 0x00007ffff7e2d367 gdt::Function<void ()>::ErasedCopyLarge<flare::Start(int, char**, gdt::Function<int (int, char**)>)::{lambda()#1}>(void*, flare::Start(int, char**, gdt::Function<int (int, char**)>)::{lambda()#1}&&)::{lambda(gdt::Function<void ()>::TypeOps const*)#1}::_FUN(gdt::Function<void ()>::TypeOps const*) + 87 [./common/base/function.h:125]
#13 0x00007ffff7309357 flare::fiber::detail::(anonymous namespace)::FiberProc(void*) + 215 [./common/base/function.h:87]

Found 6 fiber(s) in total.
```

`list-fibers-compact`输出示例（可能的情况下fiber的调用栈输出时被合并）：

```gdb
(gdb) list-fibers-compact
Fiber #1:
#0 0x00007fffefba3945 pthread_cond_wait@@GLIBC_2.3.2 + 197 [(Unknown)]
#1 0x00007ffff1a8372b gdt::PollThread::Stop() + 171 [common/system/io_frame/poll_thread.cc:53]
#2 0x00007ffff22e3673 gdt::HttpServer::Stop() + 339 [common/net/http/server/http_server.cc:347]
#3 0x0000000000427dd2 flare::protobuf::ProtoOverHttpProtocolStreamingRpc_ServerSideGdt_Test::TestBody() + 1906 [flare/rpc/protobuf/proto_over_http_protocol_streaming_rpc_test.cc:304]
#4 0x00007ffff07eb8fe void testing::internal::HandleExceptionsInMethodIfSupported<testing::Test, void>(testing::Test*, void (testing::Test::*)(), char const*) + 78 [thirdparty/googletest-1.8.1/googletest/src/gtest.cc:2424]
#5 0x00007ffff07d2dcf testing::Test::Run() + 287 [thirdparty/googletest-1.8.1/googletest/src/gtest.cc:2517]
#6 0x00007ffff07d2f70 testing::TestInfo::Run() + 400 [thirdparty/googletest-1.8.1/googletest/src/gtest.cc:2693]
#7 0x00007ffff07d3085 testing::TestCase::Run() + 261 [thirdparty/googletest-1.8.1/googletest/src/gtest.cc:2811]
#8 0x00007ffff07dfe64 testing::internal::UnitTestImpl::RunAllTests() + 1764 [/usr/local/tools/gcc/include/c++/8.2.0/bits/stl_vector.h:930]
#9 0x00007ffff07eb0ee bool testing::internal::HandleExceptionsInMethodIfSupported<testing::internal::UnitTestImpl, bool>(testing::internal::UnitTestImpl*, bool (testing::internal::UnitTestImpl::*)(), char const*) + 78 [thirdparty/googletest-1.8.1/googletest/src/gtest.cc:2424]
#10 0x00007ffff07d2358 testing::UnitTest::Run() + 168 [./thirdparty/googletest-1.8.1/googletest/include/googletest/gtest/gtest.h:1340]
#11 0x00007ffff7e5a3b1 gdt::Function<int (int, char**)>::ErasedCopySmall<flare::testing::InitAndRunAllTests(int*, char**)::{lambda(auto:1, auto:2)#1}>(void*, flare::testing::InitAndRunAllTests(int*, char**)::{lambda(auto:1, auto:2)#1}&&)::{lambda(gdt::Function<int (int, char**)>::TypeOps const*, int&&, char**&&)#1}::_FUN(gdt::Function<int (int, char**)>::TypeOps const*, flare::testing::InitAndRunAllTests(int*, char**)::{lambda(auto:1, auto:2)#1}&&, int&&) + 33 [./thirdparty/googletest-1.8.1/googletest/include/googletest/gtest/gtest.h:2341]
#12 0x00007ffff7e2d367 gdt::Function<void ()>::ErasedCopyLarge<flare::Start(int, char**, gdt::Function<int (int, char**)>)::{lambda()#1}>(void*, flare::Start(int, char**, gdt::Function<int (int, char**)>)::{lambda()#1}&&)::{lambda(gdt::Function<void ()>::TypeOps const*)#1}::_FUN(gdt::Function<void ()>::TypeOps const*) + 87 [./common/base/function.h:125]
#13 0x00007ffff7309357 flare::fiber::detail::(anonymous namespace)::FiberProc(void*) + 215 [./common/base/function.h:87]

Fiber #524446:
#0 0x00007ffff7308e65 flare::fiber::detail::FiberEntity::Resume() + 117 [flare/fiber/detail/fiber_entity.cc:186]
#1 0x00007ffff730909a flare::fiber::detail::FiberEntity::ResumeOn(gdt::Function<void ()>) + 218 [flare/fiber/detail/fiber_entity.cc:204]
#2 0x00007ffff731ecb0 flare::fiber::detail::SchedulingGroup::Halt(flare::fiber::detail::FiberEntity*, std::unique_lock<flare::Spinlock>&&) + 176 [./common/base/function.h:87]
#3 0x00007ffff732875f flare::fiber::detail::ConditionVariable::wait_until(std::unique_lock<flare::fiber::detail::Mutex>&, std::chrono::time_point<std::chrono::_V2::steady_clock, std::chrono::duration<long, std::ratio<1l, 1000000000l> > >) + 639 [flare/fiber/detail/waitable.cc:275]
#4 0x00007ffff7328aab flare::fiber::detail::ConditionVariable::wait(std::unique_lock<flare::fiber::detail::Mutex>&) + 27 [flare/fiber/detail/waitable.cc:251]
#5 0x00007ffff7654544 flare::fiber::WorkQueue::WorkerProc() + 1140 [/usr/local/tools/gcc/include/c++/8.2.0/bits/stl_deque.h:1367]
#6 0x00007ffff7654684 gdt::Function<void ()>::ErasedCopySmall<flare::fiber::WorkQueue::WorkQueue()::{lambda()#1}>(void*, flare::fiber::WorkQueue::WorkQueue()::{lambda()#1}&&)::{lambda(gdt::Function<void ()>::TypeOps const*)#1}::_FUN(gdt::Function<void ()>::TypeOps const*) + 20 [flare/fiber/work_queue.cc:15]
#7 0x00007ffff7309357 flare::fiber::detail::(anonymous namespace)::FiberProc(void*) + 215 [./common/base/function.h:87]

Fiber #131076, #131075, #131074, #131073:
#0 0x00007fffef423933 epoll_wait + 51 [(Unknown)]
#1 0x00007ffff74ce928 flare::EventLoop::WaitAndRunEvents(std::chrono::duration<long, std::ratio<1l, 1000l> >) + 56 [/usr/local/tools/gcc/include/c++/8.2.0/chrono:510]
#2 0x00007ffff74cedad flare::EventLoop::Run() + 205 [flare/io/event_loop.cc:197]
#3 0x00007ffff74cf130 gdt::Function<void ()>::ErasedCopySmall<flare::StartAllEventLoops()::{lambda()#1}&>(void*, flare::StartAllEventLoops()::{lambda()#1}&)::{lambda(gdt::Function<void ()>::TypeOps const*)#1}::_FUN(gdt::Function<void ()>::TypeOps const*) + 112 [/usr/local/tools/gcc/include/c++/8.2.0/bits/unique_ptr.h:342]
#4 0x00007ffff7309357 flare::fiber::detail::(anonymous namespace)::FiberProc(void*) + 215 [./common/base/function.h:87]

Found 6 fiber(s) in total.
```

## 技术细节

### 枚举fiber

我们通过暴力搜索内存来实现枚举所有的fiber的运行时栈并从中提取数据恢复现场。

在分配fiber运行时栈的时候，我们会控制将栈顶分配在1MB的边界上（参考[flare/fiber/detail/stack_allocator.cc](../fiber/detail/stack_allocator.cc)）。这种分配方式控制了我们所需要搜索的空间大小。

另一方面，许多内存占用较大的服务，往往是词典等资源占用了较多的内存空间，对于使用文件映射方式映射的词典，我们在搜索时会因为这个内存段有相应的文件映射而跳过，进一步减少搜索空间。

对于所有可能的地址（存在匿名映射的1MB边界），我们会读取一个页（通常是4KiB）的数据，并比对其是否包含我们的magic number（参见[flare/fiber/detail/fiber_entity.h](../fiber/detail/fiber_entity.h)中的`FiberEntity::magic`），如果包含，则认为这是一个fiber运行时栈的栈顶并记录这个fiber的`FiberEntity`的指针。

整体性能表现而言，在Intel Xeon Gold 6133上的KVM虚拟机内，对于100GB的不包含fiber的连续内存，一次暴力搜索内存可以在~0.4s内完成。这对于我们的实际使用场景是可以接受的。

### 过滤“非常规”fiber

暴力搜索内存可能会搜索到已经退出的fiber，这些fiber的栈会被对象池回收，因此仍然可以搜索到，但是我们不应当将其展示出来。

我们的实现会按照`FiberEntity::state`过滤掉这些fiber。

另外，每个pthread有一个相关联的"master fiber"，这些"master fiber"的栈大小为0，我们也会将之过滤掉。

### 恢复调用栈

对于挂起的fibers，[jump_context](../fiber/detail/x86_64/jump_context.S) / [make_context](../fiber/detail/x86_64/make_context.S)会将fiber的上下文保存至`FiberEntity::state_save_area`指向的内存块（fiber被挂起/初始化时的栈顶）。我们会根据保存状态时的格式，将`rip`、`rsp`、`rbp`解析/推导出来。

对于正在运行的fibers，从`FiberEntity::state_save_area`处得到的数据并不能反映fiber最新的执行状况。但是对于这些fibers，我们可以从正在运行它的pthread worker的上下文中得到其最新的执行状况。我们会通过gdb内置方法获取所有正在运行的pthread的上下文，如果某个pthread当前的`rsp`存在于某个fiber的栈内部，那么就会用这个pthread的上下文（包括但不限于我们所需要的`rip`、`rsp`、`rbp`）来替换我们从`FiberEntity::state_save_area`读取到的数据以供后面使用。

*这儿有一种边界情况：pthread worker正在切换fiber（体现为正在执行`jump_context`）。这种情况下取决于当前`jump_context`执行到什么位置，pthread worker的上下文可能反映、也可能不反映其`rsp`所对应的fiber的上下文。具体来说，若`jump_context`正在保存上下文，那么必须使用pthread worker的上下文来进行处理，否则（保存完毕，新的fiber正在被加载但`jump_context`尚未返回）无论`rsp`属于哪一个fiber，都应该使用`state_save_area`处的数据进行处理。目前我们尚未特别处理这种边界情况，但是会输出`Warning: The fiber is being swapped in / out, the call stack shown here can be wrong.`以作提示。*

在得到了`rip`、`rsp`、`rbp`之后，我们[根据`rbp`展开调用栈](https://en.wikipedia.org/wiki/Call_stack#Stack_and_frame_pointers)，以此获取调用方、调用方的调用方、调用方的调用方的调用方、...的`rip`、`rsp`、`rbp`并得到完整的调用栈。

在得到了每层栈的`rip`之后，我们通过gdb的内置方法，将其解析为函数名、文件名、行号等信息并展示。

---
[返回目录](README.md)
