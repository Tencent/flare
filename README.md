# Flare 后台服务开发框架

Flare 是一个现代化的后台服务开发框架，旨在提供针对目前主流软硬件环境下的易用、高性能、平稳的服务开发能力。

目前 Flare 广泛投产于腾讯广告，并拥有数以万计的运行实例。

## 开始使用

Flare 已经自带了所需的[第三方库](thirdparty/)，因此通常不需要额外安装依赖库。

为了编译 Flare，需要GCC 8或更新版本的支持。

### 开发

我们使用[`blade`](https://github.com/chen3feng/blade-build)进行日常开发。

- 编译：`./blade build ...`，
- 测试：`./blade test ...`。

之后就可以参考[入门导引](flare/doc/intro-rpc.md)中的介绍，搭建一个简单的RPC服务了。

### 调试

我们相信，[调试](flare/doc/debugging.md)体验也是开发维护过程中很重要的一部分，我们为此也做了如下一些支持：

- [GDB插件用于列出挂起的fibers](flare/doc/gdb-plugin.md)
- [支持各种Sanitizers](flare/doc/sanitizers.md)

### 测试

为了改善编写单测的体验，我们提供了一些用于[编写单测的工具](flare/doc/testing.md)。

这包括但不限于：

- RPC Mock
- CKV Mock
- HTTP Mock
- 非虚函数Mock
- 部分工具方法等

## 二次开发

另外，对于希望了解 Flare 更多内部设计的开发者，或需要对 Flare 进行二次开发的开发者而言，[`flare/doc/`](flare/doc/)下有更多的技术文档可供参考。

## 性能

虽然我们在设计过程中更倾向于优化延迟而非吞吐，但是出于简单的对比目的，我们提供了[初步的性能数据](flare/doc/benchmark.md)。

## 致谢

- 我们的底层实现大量参考了[brpc](https://github.com/apache/incubator-brpc)的设计；
- RPC部分[grpc](https://grpc.io/)给了我们很多启发。

在此，我们对上述项目一并致以谢意。
