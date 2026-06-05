# HTTP

flare将HTTP作为一种内置协议提供支持，注意这里特指**原生**的HTTP（[RFC 7230](https://datatracker.ietf.org/doc/html/rfc7230)及其后续）请求，而非由HTTP承载的Protocol Buffers协议。后者属于[Protocol Buffers](protocol-buffers.md)中描述的线上格式之一。

完整的用户侧接口（包括`HttpHandler`、`NewHttpXxxHandler`、`HttpRequest`、`HttpResponse`等）请参见[HTTP入门导引](../intro-http.md)。

## 实现概要

- [`Http11Protocol`](../../rpc/protocol/http/http11_protocol.h)是HTTP/1.x的[`StreamProtocol`](../../rpc/protocol/stream_protocol.h)实现，负责从`NoncontiguousBuffer`中切分并解析HTTP消息。该实现通过`FLARE_RPC_REGISTER_SERVER_SIDE_STREAM_PROTOCOL_ARG("http", Http11Protocol, ...)`注册到框架，因此`Server::AddProtocol("http")`即可启用。
- [`HttpService`](../../rpc/protocol/http/service.h)是相应的[`StreamService`](../../rpc/protocol/stream_service.h)实现，根据请求URI找到对应的`HttpHandler`进行分发。
- [`HttpRequest`](../../net/http/http_request.h) / [`HttpResponse`](../../net/http/http_response.h)是面向用户的消息类型，[`HttpStatus`](../../net/http/types.h)定义了常用的状态码枚举（如`HttpStatus::OK`）。
- 客户端的HTTP接口请参见[`HttpClient`](../../net/http/http_client.h)，与`RpcChannel`使用方式不同——HTTP客户端不需要预先`Open`连接。

## URI路由

服务端通过`Server::AddHttpHandler("/path/to/svc", handler)`注册路径，详见[HTTP入门导引](../intro-http.md)。

## 内置`/inspect/...`

框架内部还通过HTTP协议暴露了若干运行时观察接口（如[表单提交](../rpc-form.md)、[监控](../monitoring.md)等），相关实现位于[`flare/rpc/protocol/http/builtin/`](../../rpc/protocol/http/builtin/)。这些接口在调用`Server::AddProtocol("http")`时自动启用。

---
[返回目录](../README.md)
