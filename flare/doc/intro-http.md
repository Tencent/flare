# HTTP 入门导引

flare除支持[RPC 入门导引](intro-rpc.md)中介绍的使用[Protocol Buffers](https://github.com/protocolbuffers/protobuf)开发 RPC 服务外，同样支持开发 HTTP 服务。

## Echo服务开发

flare中对HTTP的请求的处理通过[`HttpHandler`](../rpc/http_handler.h)完成。`HttpHandler`可以关联至某个[URI](https://en.wikipedia.org/wiki/Uniform_Resource_Identifier)上，后续框架会将相应的请求转发给这一`HttpHandler`。

*目前我们也支持将`HttpHandler`关联至一组（通过[std::regex](https://en.cppreference.com/w/cpp/regex)匹配，后续可能支持[Hyperscan](https://github.com/intel/hyperscan)等）URI，但暂时限内部使用。*

### 实现业务逻辑

我们提供了如下两种方式实现具体的业务逻辑：

- 通过继承`HttpHandler`实现。这种实现方式一般适合于相对复杂的业务逻辑。对于这种情况，可以将之定义为一个`HttpHandler`的子类来单独实现。
- 通过[lambda表达式](https://en.cppreference.com/w/cpp/language/lambda)实现。对于相对简单的接口，我们也允许业务直接通过lambda表达式实现避免引入过多零碎的微小的类。

#### 通过`HttpHandler`实现

`HttpHandler`定义如下（省略部分不影响使用的细节）：

```cpp
class HttpHandler {
 public:
  virtual void HandleRequest(const HttpRequest& request, HttpResponse* response,
                             HttpServerContext* context);

  virtual void OnGet(const HttpRequest& request, HttpResponse* response,
                     HttpServerContext* context);
  virtual void OnHead(const HttpRequest& request, HttpResponse* response,
                      HttpServerContext* context);
  virtual void OnPost(const HttpRequest& request, HttpResponse* response,
                      HttpServerContext* context);
  virtual void OnPut(const HttpRequest& request, HttpResponse* response,
                     HttpServerContext* context);
  virtual void OnDelete(const HttpRequest& request, HttpResponse* response,
                        HttpServerContext* context);
  virtual void OnConnect(const HttpRequest& request, HttpResponse* response,
                         HttpServerContext* context);
  virtual void OnOptions(const HttpRequest& request, HttpResponse* response,
                         HttpServerContext* context);
  virtual void OnTrace(const HttpRequest& request, HttpResponse* response,
                       HttpServerContext* context);
  virtual void OnPatch(const HttpRequest& request, HttpResponse* response,
                       HttpServerContext* context);
};
```

取决于具体业务逻辑，业务应当实现如下**二者之一**，但不应同时实现：

- `HandleRequest`
- 一个或多个`OnXxx`

*内部而言，默认的`HandleRequest`实现会根据请求中的[方法](https://developer.mozilla.org/en-US/docs/Web/HTTP/Methods)调用`OnXxx`。因此，如果业务实现了自己的`HandleRequest`，那么`OnXxx`将不会被调用。*

其中[`HttpRequest`](../net/http/http_request.h)、[`HttpResponse`](../net/http/http_response.h)用于业务读取、写出响应，[`HttpServerContext`](../rpc/protocol/http/http_server_context.h)提供了部分带外信息（如收到请求的时间戳等）。

特别的，若业务未手动指定，则框架会负责填写如下[HTTP头](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers)：

- [`Connection`](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Connection)：保持和`HttpRequest::IsKeepAlive`一致。
- [`Content-Length`](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Content-Length)：填写为`HttpResponse::body()`的字节长度。
- [`Content-Encoding`](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Content-Encoding)：服务端并不强制一定使用客户端指定的压缩[`Accept-Encoding`](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Accept-Encoding), 现在也没有对消息主体使用任何压缩模式, 所以不设置此字段。

**业务代码需要自行通过`HttpResponse::set_status`指定[HTTP返回值](https://en.wikipedia.org/wiki/List_of_HTTP_status_codes)，如`HttpResponse::Status_OK`（即200）。**

因此，一个简单的Echo服务（假设通过[`GET`](https://developer.mozilla.org/en-US/docs/Web/HTTP/Methods/GET)方法请求）实现如下：

```cpp
class EchoHandler : public flare::HttpHandler {
 public:
  void OnGet(const flare::HttpRequest& request, flare::HttpResponse* response,
             flare::HttpServerContext* context) override {
    response->set_status(flare::HttpResponse::Status_OK);
    response->set_body(request.body());
  }
};
```

#### 通过lambda表达式实现

我们也提供了一系列[`NewHttpXxxHandler`](../rpc/http_handler.h)用于直接将lambda表达式包装为`HttpHandler`的子类对象：

- `NewHttpGetHandler`
- `NewHttpHeadHandler`
- `NewHttpPostHandler`
- `NewHttpPutHandler`
- `NewHttpDeleteHandler`
- `NewHttpConnectHandler`
- `NewHttpOptionsHandler`
- `NewHttpTraceHandler`
- `NewHttpPatchHandler`

*我们实际上也提供了`http::NewHttpHandler`，目前仅供内部使用。对于需要处理多种HTTP方法的请求，通常其实现逻辑之复杂度值得将之作为单独的类实现。*

这些方法均具备如下接口：

```cpp
template <class F> std::unique_ptr<HttpHandler> NewHttpXxxHandler(F&& f);
```

其中`F`对于参数`const HttpRequest&, HttpResponse*, HttpServerContext*`而言需要满足[`Callable`](https://en.cppreference.com/w/cpp/named_req/Callable)这一概念，并返回`void`。

*简单来说，即是说`F`需要有类似于`[] (const HttpRequest&, HttpResponse*, HttpServerContext*) { /* ... */}`的定义。*

这些`NewHttpXxxHandler`返回的类型（`std::unique_ptr<HttpHandler>`）可以直接向框架（即[`Server`](../rpc/server.h)）注册（见后文）。

使用`NewHttpGetHandler`实现的Echo接口如下：

```cpp
auto handler = flare::NewHttpGetHandler([](auto&& r, auto&& w, auto&& c) {
  w->set_status(flare::HttpResponse::Status_OK);
  w->set_body(r.body());
};
```

### 向框架注册

[`Server`](../rpc/server.h)提供了如下方法用于注册`HttpHandler`：

```cpp
void AddHttpHandler(std::string path, MaybeOwning<http::HttpHandler> handler);
```

[`MaybeOwning<T>`](../base/maybe_owning.h)可以隐式的由[`std::unique_ptr<T>`](https://en.cppreference.com/w/cpp/memory/unique_ptr)构造，或手动构造，具体可参见该类的注释。

如对于上文中的`EchoHandler`，可以通过如下方式注册并启动服务（部分逻辑同[Protocol Buffers的Echo服务](intro-rpc.md)，不再赘述）：

```cpp
namespace example::http_echo {

int Entry(int argc, char** argv) {
  flare::Server server;

  server.AddProtocol("http");
  server.AddHttpHandler("/path/to/echo.svc", std::make_unique<EchoHandler>());
  server.ListenOn(flare::EndpointFromIpv4("127.0.0.1", 8888));
  FLARE_CHECK(server.Start());

  flare::WaitForQuitSignal();
  server.Stop();
  server.Join();

  return 0;
}

}  // namespace example::http_echo
```

对于通过`NewHttpXxxHandler`实现的逻辑，也可通过相同方式注册：

```cpp
server->AddHttpHandler("/path/to/echo2", std::move(handler));
// Or:
server->AddHttpHandler("/path/to/echo3",
                    flare::NewHttpGetHandler([](auto&& r, auto&& w, auto&& c) {
                      w->set_status(flare::HttpResponse::Status_OK);
                      w->set_body("Echo from a fancy lambda: " + r.body());
                    }));
```

### 启动服务

最后需要在`main`中通过[`Start(...)`](../init.h)启动服务：

```cpp
int main(int argc, char** argv) {
  return flare::Start(argc, argv, example::http_echo::Entry);
}
```

### 可选功能

这一节列出了部分对HTTP服务而言非必须的可选功能。

#### `HttpFilter`

某些场景下可能需要统一的对请求进行过滤（如鉴权等），我们允许服务实现方通过`Server::AddHttpFilter`来注册一个或多个[`HttpFilter`](../rpc/http_filter.h)。

需要注意的是，这些filter**只会对HTTP请求生效，对于“类似但不是”HTTP的请求（如某些基于HTTP协议的Protocol Buffers请求）不会生效**。

```cpp
server->AddHttpFilter(std::make_unique<XxxFilter>(...));
```

这些`HttpFilter`可以修改请求、响应等信息，它们将会按照注册的顺序被执行。

另外，我们也内置了一些相对通用的`HttpFilter`：

- [`NetworkLocationAllowOnHitHttpFilter`、`NetworkLocationBlockOnHitHttpFilter`](../rpc/builtin/network_location_http_filter.h)：命中名单中的IP时允许/禁止访问，即基于IP的黑白名单。禁止访问时返回HTTP 403。
- [`BasicAuthenticationHttpFilter`](../rpc/builtin/basic_authentication_http_filter.h)：这个filter使用请求的HTTP请求包头的`Authorization`字段解析请求方提供的账号密码，并通过用户提供的回调决定是否接受请求。对于拒绝的请求，这个过滤器返回HTTP 401。

## Http客户端

`flare`中可以通过[`HttpClient`](../net/http/http_client.h)对`Http`服务端进行`Http`请求。
可以通过调用`Get`, `Post`方法快速进行请求, 也可以通过`Request`方法使用自己构造的`HttpRequest`。
方法均支持同步/异步两种调用方式。

### 实现原理

`Http`客户端实现基于第三方库[`libcurl`](https://curl.haxx.se/libcurl/), 在第一次构造`HttpClient`时, 根据[调度组](./scheduling-group.md)数量启动一批后台`worker`线程执行`libcurl`的工作事件循环, `worker`线程数量为调度组数量乘以`flag`变量`flare_http_client_workers_per_scheduling_group`.

### 连接管理

Http客户端默认与服务端保持长连接, 除非服务端响应中设置`Connection`头为`close`指定关闭连接。

每个后台`worker`的总连接数通过`flare_http_client_max_total_connections_per_worker`控制,
对每个`Host`的最大连接数由`flare_http_client_max_connections_per_host_per_worker`控制。

### 重定向

`HttpClient`支持客户端重定向, 可以通过`HttpClient::Options`的`follow_redirects`选择是否开启(默认开启), 以及`RequestOptions::max_redirection_count`设置单次请求最大的重定向次数。

请求调用时可以传入`ResponseInfo`指针得到最后实际请求的`url`.

```cpp
TEST_F(HttpClientTest, Redirect) {
  // test too many redirect
  HttpClient::RequestOptions request_options;
  request_options.max_redirection_count = 1;
  HttpClient::ResponseInfo response_info;
  HttpClient client;

  auto resp =
      client.Get(site_url_ + "redirect", request_options, &response_info);
  EXPECT_EQ(HttpClient::ERROR_TOO_MANY_REDIRECTS, resp.error());
}
```

### HTTPS

`HttpClient`支持`Https`协议, 需明确指定请求url的`scheme`为`https`, 使用1.0.2版本的`OpenSsl`库。

可以通过设置`HttpClient::Options`的`verify_server_certificate`成员来控制是否对服务端的证书进行校验, 默认开启。

```cpp
TEST(HttpClient, Https) {
  HttpClient::Options opts;
  opts.verify_server_certificate = false;
  HttpClient client(opts);
  auto res = client.Get("https://example.com/");
  EXPECT_TRUE(res);
  EXPECT_EQ(HttpStatus(200), res->status());
}
```

### HTTP版本

支持`Http1.0`, `Http1.1`及`Http2`。可以通过设置`RequestOptions`中的`http_version`来设置请求的版本, 默认`HttpClient`将会选择合适的版本, 可以从`ResponseInfo`获取实际使用的`http_version`。如果事先已知道服务端支持`HTTP2`可以设置`RequestOptions`的`no_automatic_upgrade`为true来直接使用`HTTP2`而不是通过先`HTTP1.1 upgrade`的方式。

```cpp
TEST(HttpClient, DISABLED_Http2) {
  HttpClient client;
  HttpClient::RequestOptions req_opts;
  req_opts.timeout = 5s;
  HttpClient::ResponseInfo info;
  auto&& resp = client.Get("https://nghttp2.org/", req_opts, &info);
  EXPECT_TRUE(resp);
  EXPECT_EQ(HttpStatus::OK, resp->status());
  EXPECT_EQ(HttpVersion::V_2, info.http_version);
}
```

### 代理

`HttpClient`支持客户端代理, 代理默认开启, 从环境变量中获取代理地址, 可以通过
`HttpClient::Options`的`proxy_from_env`或者`proxy`字段设置。

当`proxy_from_env`设为`true`时, 将从环境变量`no_proxy`中匹配是否使用代理, 如果开启再根据协议从`http_proxy`或`https_proxy`中得到代理地址。

当`proxy_from_env`为默认值`false`时, 可以通过设置`proxy`字段指定特定的代理地址。

```cpp
TEST(HttpClient, DISABLED_TestProxy) {
  HttpClient client;
  HttpClient::RequestOptions req_opts;
  req_opts.proxy_from_env = true;
  req_opts.headers = {"User-Agent: curl/7.29.0"};
  auto res = client.Get("https://www.tencent.com/", req_opts);
  EXPECT_TRUE(res);
  EXPECT_EQ(HttpStatus(200), res->status());
}
```

### URL使用名字服务地址

标准`http`请求`url`不支持名字服务地址做负载均衡, 我们扩展支持了`url`中的`Host`字段使用名字服务地址, 目前支持`L5`以及`Polaris`两种方式, 需要在`request_options`中的`override_host_nslb`指定使用哪种名字服务.

```cpp
flare::HttpClient client;

{
  // L5 example
  flare::HttpClient::RequestOptions opts{
    .override_host_nslb = "cl5",
  };
  auto resp = client.Get("http://123-456/blabla", opts);
}

{
  // Polaris example
  flare::HttpClient::RequestOptions opts{
    .override_host_nslb = "polaris",
  };
  auto resp = client.Get("http://cdg.ads@Production/blabla", opts);
}
```

### 压缩/解压缩

`Server`及`HttpClient`支持对`Http`响应`Body`压缩/解压缩。

通过设置`HttpClient::Options`的`use_builtin_compression`开启使用内置的压缩。
开启后, 内部会在`Http`请求`Headers`中增加请求头`Accept-Encoding`设置支持的压缩方法`identity, gzip, deflate`, 如果响应中使用了压缩, 客户端会自动解压缩。

自动解压缩后, `response`的消息体已被解压缩, 但是`header`头部依然包含原始的`Content-Encoding`字段。

如果你不想要服务端压缩响应, 或者你希望服务端使用其他的压缩算法, 或者你想自己解压响应消息体, 你应该保留`use_builtin_compression`为`false`, 并且设置请求`Headers`中的字段`Accept-Encoding`。此时`HttpClient`将不会对响应体进行解压, 哪怕你希望和服务端实际使用的压缩算法是`gzip`。

```cpp
TEST_F(HttpClientTest, Compression) {
  HttpClient::Options opts;
  opts.use_builtin_compression = true;
  HttpClient client1(opts);
  opts.use_builtin_compression = false;
  HttpClient client2(opts);
  HttpClient::RequestOptions req_opts;
  auto&& resp =
      client1.Post(site_url_ + "get_wanted_num_of_bytes", "10"sv, req_opts);
  EXPECT_TRUE(resp);
  EXPECT_EQ("gzip"sv, *resp->headers()->TryGet("Content-Encoding"));
  EXPECT_EQ(std::string(10, 'A'), *resp->body());
  resp = client2.Post(site_url_ + "get_wanted_num_of_bytes", "10"sv, req_opts);
  EXPECT_TRUE(resp);
  EXPECT_FALSE(resp->headers()->TryGet("Content-Encoding"));
  EXPECT_EQ(std::string(10, 'A'), *resp->body());
  req_opts.headers = {"Accept-Encoding: snappy"};
  resp = client2.Post(site_url_ + "get_wanted_num_of_bytes", "10"sv, req_opts);
  // Decompress ourselve
  EXPECT_TRUE(resp);
  EXPECT_EQ("snappy"sv, *resp->headers()->TryGet("Content-Encoding"));
  auto&& decompressed =
      Decompress(MakeDecompressor("snappy").get(), *resp->body());
  EXPECT_EQ(std::string(10, 'A'), FlattenSlow(*decompressed));
}
```

### 调试功能

如果你希望看到请求发送以及接收响应的全过程, 你可以在请求的`RequestOptions`中设置`verbose`为`true`, 详细的请求过程将会输出至`stderr`。

```cpp
TEST(HttpClient, DISABLED_TestProxy) {
  HttpClient client;
  HttpClient::RequestOptions req_opts;
  req_opts.verbose = true;
  req_opts.headers = {"User-Agent: curl/7.29.0"};
  auto res = client.Get("https://www.tencent.com/", req_opts);
  EXPECT_TRUE(res);
  EXPECT_EQ(HttpStatus(200), res->status());
}
```

### 分块传输编码(chunked transfer encoding)

支持服务端返回分块传输编码,客户端会等待分块全部返回后再返回给用户, 即调用完成后的`response`中的`body`已拼接好并是完整的。

返回后的`response`中的头部依然包含原始的`Transfer-Encoding`字段。

```cpp
TEST_F(HttpClientTest, Chunked) {
  HttpClient client;
  auto&& resp = client.Get(site_url_ + "chunked");
  EXPECT_TRUE(resp);
  EXPECT_EQ("122333444455555", *resp->body());
  EXPECT_EQ(*resp->headers()->TryGet("Transfer-Encoding"), "chunked");
}
```

### 尚不支持的功能

Pipelining.

### 其他

同[intro-rpc.md](intro-rpc.md)，服务在提供HTTP服务的同时，也可以通过`AddProtocol`添加更多的协议来支持其他协议，此处不再赘述。
[example/](../example/)下面可以找到更多的示例代码（[example/http](../example/http)、[example/rpc/mixed_echo](../example/rpc/mixed_echo)等）。

---
[返回目录](README.md)
