# Flare 后台服务开发框架

[![license NewBSD](https://img.shields.io/badge/license-BSD-yellow.svg)](LICENSE)
[![Python](https://img.shields.io/badge/language-C++-blue.svg)](https://isocpp.org/)
[![Code Style](https://img.shields.io/badge/code%20style-google-blue.svg)](https://google.github.io/styleguide/cppguide.html)
[![Platform](https://img.shields.io/badge/platform-linux%20-lightgrey.svg)](doc/en/prerequisites.md)

[腾讯广告](https://e.qq.com/ads/) 是腾讯公司最重要的业务之一，其后台大量采用 C++ 开发。

Flare 是我们吸收先前服务框架和业界开源项目及最新研究成果开发的现代化的后台服务开发框架，旨在提供针对目前主流软硬件环境下的易用、高性能、平稳的服务开发能力。

Flare 项目开始于 2019 年，目前广泛应用于腾讯广告的众多后台服务，拥有数以万计的运行实例，在实际生产系统上经受了足够的考验。

2021 年 5 月，本着回馈社区、技术共享的精神，正式对外开源。

## 特点

- 现代 C++ 设计风格，广泛采用了 C++11/14/17 的新的语法特性和标准库
- 提供了 [M:N 的线程模型](https://en.wikipedia.org/wiki/Thread_(computing))的微线程实现[Fiber](flare/doc/fiber.md)，方便业务开发人员以便利的同步调用语法编写高性能的异步调用代码
- 支持基于消息的[流式 RPC](flare/doc/streaming-rpc.md)支持
- 除了 RPC 外，还提供了一系列便利的[基础库](flare/base)，比如字符串、时间日期、编码处理、压缩、加密解密、配置、HTTP 客户端等，方便快速上手开发业务代码
- 提供了灵活的扩充机制。方便支持多种协议、服务发现、负载均衡、监控告警、[调用追踪](flare/doc/)等
- 针对现代体系结构做了大量的优化。比如 [NUMA 感知](https://en.wikipedia.org/wiki/Non-uniform_memory_access)的[调度组](flare/)和[对象池](flare/doc/object-pool.md)、[零拷贝缓冲区](flare/doc/buffer.md)等
- 高质量的代码。严格遵守 [Google C++ 代码规范](https://google.github.io/styleguide/cppguide.html)，测试覆盖率达 80%。
- 完善的[文档](flare/doc)和[示例](flare/example)以及[调试支持](flare/doc/debugging.md)，方便快速上手。

## 系统要求

- Linux 3.10 及以上内核，暂不支持其他操作系统
- x86-64 处理器，也支持 aarch64 及 ppc64le，但是未在生产环境上实际使用过
- GCC 8 及以上版本的编译器

## 开始使用

Flare 是开箱即用的，已经自带了所需的[第三方库](thirdparty/)，因此通常不需要额外安装依赖库。

为了编译 Flare，需要GCC 8或更新版本的支持。

### 构建

我们使用[`blade`](https://github.com/chen3feng/blade-build)进行日常开发。

- 编译：`./blade build ...`，
- 测试：`./blade test ...`。

之后就可以参考[入门导引](flare/doc/intro-rpc.md)中的介绍，搭建一个简单的RPC服务了。
我们还提供了[示例](flare/example)。

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

我们非常欢迎参与共同建设，对于希望了解 Flare 更多内部设计的开发者，或需要对 Flare 进行二次开发的开发者而言，[`flare/doc/`](flare/doc/)下有更多的技术文档可供参考。
详情请参考[CONTRIBUTING.md](CONTRIBUTING.md)。

## 性能

由于业务需求的特点，我们在设计过程中更倾向于优化延迟及抖动的平稳性而非吞吐，但是也在这个前提下尽量保证性能。
出于简单的对比目的，我们提供了[初步的性能数据](flare/doc/benchmark.md)。

## 致谢

- 我们的底层实现大量参考了[brpc](https://github.com/apache/incubator-brpc)的设计。
- RPC部分，[grpc](https://grpc.io/)给了我们很多启发。
- 我们依赖了不少开源社区的[第三方库](thirdparty/)，站在巨人的肩膀上使得我们可以更快更好地开发本项目，也因此积极地回馈给开源社区。

在此，我们对上述项目一并致以谢意。
