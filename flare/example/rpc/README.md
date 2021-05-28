# RPC 服务框架示例

## 基本示例

演示 RPC 框架客户端和服务器端的基本使用方法，建议按顺序学习。

- [server.cc](server.cc) 服务器端示例，演示基本的服务器如何实现
- [client.cc](client.cc)客户端示例，演示客户端如何调用服务器端
- [client2.cc](client2.cc) 客户端示例，演示客户端如何调用服务器端，更复杂一些的场景
- [relay_server.cc](relay_server.cc) 中继服务器示例，演示在服务中请求其他服务
- [async_client.cc](async_client.cc) 异步客户端，演示基于 [Future](../../doc/future.md) 的客户端调用

## 其他示例和测试工具

- [press.cc](press.cc) 压力测试客户端
- [press2.cc](press2.cc) 压力测试客户端

## 其它子目录

演示对其他协议的支持等

- [mixed_echo](mixed_echo) 混合 PB-RPC/HTTP 协议
- [brpc](brpc) 百度 BRPC 协议服务
- [hbase](hbase) HBASE RPC 协议
