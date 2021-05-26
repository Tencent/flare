# 测试

这篇文档描述了一些Flare提供的用于改善代码可测性的能力。

## 在Flare环境中运行UT

需要注意的是，对于需要使用到Flare的逻辑的UT，通常会需要先初始化Flare运行时再执行UT。为了简化使用，避免UT每次编写代码初始化Flare、gtest再运行UT，我们提供了宏[`FLARE_TEST_MAIN`](../testing/main.h)来完成这一工作。

UT需要增加依赖目标`//flare/testing:main`：

```python
cc_test(
  name = 'some_test',
  srcs = 'some_test.cc',
  deps = [
    # Other deps.
    '//flare/testing:main`
  ]
)
```

之后在UT文件底部增加宏`FLARE_TEST_MAIN`即可。这一宏会展开为形如`int main(int argc, char** argv) { ... }`的定义。具体的实际定义可以参考[源代码](../testing/main.h)。

```cpp
#include "flare/testing/main.h"

namespace example {

TEST(TestExample, Example1) {
  // ...
}

}  // namespace flare::testing::detail

FLARE_TEST_MAIN  // Expands to definition of `main`.
```

## RPC Mock

为了支持单元测试中对RPC服务的mock的需求，Flare继承了poppymock的能力，允许UT代码通过[`FLARE_EXPECT_RPC`](../testing/rpc_mock.h)来控制“后端服务”的RPC行为。

**在测试时，应当使用`mock://`作为被请求的服务的URI前缀。**

内部而言，这一能力是通过框架动态替换了[`RpcChannel`的实现](../testing/rpc_mock.h)来实现的。对于以`mock://`开头的URI，通过`flare::RpcChannel`发起的RPC调用最终都会落在`flare::MockChannel`中。

考虑到通过`XxxService_Stub`发起的RPC，最终都会由`google::protobuf::RpcChannel::CallMethod(...)`执行，而我们的实现中通过`MOCK_METHOD`将`flare::MockChannel::CallMethod`托管至了[gtest](https://github.com/google/googletest)，因此最终实现了和gtest的整合。

测试代码可以通过`FLARE_EXPECT_RPC(Method, Matchers...)`的方式来mock RPC响应。如果需要构造响应，则可以通过[`flare::testing::Return`](../testing/detail/gmock_actions.h)（作为`WillRepeatedly`系的方法的参数）以如下方式之一返回：

- `flare::testing::Return(const google::protobuf::Message& message)`：返回成功，并将`message`复制到`response`。
- `flare::testing::Return(rpc::Status status)`：返回失败，并将`status`作为具体的错误码。
- `flare::testing::Return(const std::string& desc)`：以`STATUS_FAILED`返回失败，并将`desc`作为具体的错误信息。
- `flare::testing::Return(rpc::Status status, const std::string& desc)`：以`status` + `desc`来返回失败。
- `flare::testing::Return(const flare::Status& status)`：以`status.code()` + `status.message()`来返回失败。
- `flare::testing::HandleRpc(F&& handler)`：使用用户的`handler`回调来处理请求。`handler`的参数同服务端的`XxxService::XxxMethod`。（这儿受限于技术因素，如果需要访问请求/响应，`handler`的参数需要写出实际C++类型，不能使用`auto`，否则Flare不能正确的进行类型转换。）

```cpp
TEST(MockRpc, Test1) {
  static const auto kResponse = "test body"s;
  RpcChannel channel;
  channel.Open("mock://what-ever-it-wants-to-be.");  // `scheme` of URI matters.

  // Make an RPC.
  testing::EchoService_Stub stub(&channel);
  testing::EchoRequest req;
  testing::EchoResponse resp, resp_data;
  req.set_body("not cared");
  resp_data.set_body(kResponse);

  // Mock the response.
  FLARE_EXPECT_RPC(testing::EchoService::Echo, ::testing::_)
      .WillRepeatedly(testing::Return(resp_data));

  RpcClientController ctlr;
  stub.Echo(&ctlr, &req, &resp, nullptr);
  ASSERT_FALSE(ctlr.Failed());

  // Check the mock works.
  ASSERT_EQ(kResponse, resp.body());
}

FLARE_TEST_MAIN
```

## HTTP Mock

为了支持单元测试中对HTTP服务的mock的需求，Flare允许UT代码通过[`FLARE_EXPECT_HTTP`](../testing/http_mock.h)来控制`Http Client`对`Http Server`的调用行为。

**在测试时，应当使用`mock://`作为被请求的服务的URI前缀。**

要想使用这一行为, 需要使用[`flare::HttpClient`](../net/http/http_client.h)的`Get`, `Post`或`Request`方法发起`Http`调用。

测试代码可以匹配请求中的`Url`, `Method`, `Headers`, `Body`来构造相应的`Http`响应， 下面分别给出示例。

`url`:

- `FLARE_EXPECT_HTTP(url, _, _, _).WillRepeatedly(Return...`

`Method`:

- `FLARE_EXPECT_HTTP(_, HttpMethod::Get, _, _).WillRepeatedly(Return...`

`Header`:

- `FLARE_EXPECT_HTTP(_, _, HttpHeaderContains("blabla"), _).WillRepeatedly(Return...`
- `FLARE_EXPECT_HTTP(_, _, HttpHeaderEq("blabla"), _).WillRepeatedly(Return...`

需要注意的是`Header`的匹配只对`Request`方法有效.

`Body`:

- `FLARE_EXPECT_HTTP(_, _, _, HasSubstr("blabla")).WillRepeatedly`

响应则可以通过[`flare::testing::Return`](../testing/detail/gmock_actions.h)（作为`WillRepeatedly`系的方法的参数）以如下方式之一返回：

- `flare::testing::Return(flare::HttpResponse response)`：调用成功，返回`response`。
- `flare::testing::Return(flare::HttpClient::ErrorCode error)`：调用失败，返回错误码`error`。
- `flare::testing::Return(flare::HttpResponse response, flare::HttpClient::ResponseInfo info)`：调用成功，返回`response`, 并将`info`复制到`response_info`。

```cpp
TEST(HttpMockTest, HttpMatchHeaderContain) {
  flare::HttpClient c;

  HttpResponse resp;
  std::string url = "mock://asdasd";
  HttpClient::ErrorCode err = HttpClient::ERROR_CONNECTION;
  FLARE_EXPECT_HTTP(_, _, HttpHeaderContains("aaa"), _)
      .WillRepeatedly(Return(resp));
  FLARE_EXPECT_HTTP(_, _, Not(HttpHeaderContains("aaa")), _)
      .WillRepeatedly(Return(err));
  HttpRequest req;
  req.headers()->Append("aaa", "val");
  auto&& e = c.Request("mock", "", req);
  EXPECT_TRUE(e);
  req.headers()->Remove("aaa");
  e = c.Request("mock", "", req);
  EXPECT_FALSE(e);
}

FLARE_TEST_MAIN
```

## CKV Mock

**为了能够在单测中截获CKV调用，请求CKV时需要使用`mock://...`方式的URI来指定服务器地址。**

[我们提供了一些宏](../testing/ckv_mock.h)用于解决单测对CKV的依赖：

- `FLARE_EXPECT_CKV_XXX_KEYS(bid, request_matcher)`：这类宏对应于[CKV的老式接口](protocol/ckv.md)，用于捕获`CkvClient::XxxKeys`系列接口。
- `FLARE_EXPECT_CKV_XXX_COLUMNS(bid, request_matcher)`：这类宏对应于CKV的V2系列接口，用于捕获`CkvClient::XxxColumns`。

为了匹配请求，可以使用下列表达式填入`request_matcher`（如果不需要匹配请求内容也可以直接用`::testing::_`。

- 老式接口（`CkvClient::XxxKeys`）
  - `flare::testing::CkvKeysEqual({"key1", "key2", ...})`：用于匹配请求中的key。**顺序敏感**。
  - `flare::testing::CkvKeyValuePairsEqual({{"key1", "value1"}, {"key2", "value2}})`：用于匹配请求中的KV对。**顺序敏感**。
- V2接口（`CkvClient::XxxColumns`）
  - `flare::testing::CkvColumnsEqual("key", {1, 2, 3, ...})`：用于匹配请求中的列编号。**顺序敏感**。
  - `flare::testing::CkvColumnValuePairsEqual("key", {{1, "v1"}, {2, "v2"}, {3, "v3"}, ...})`：用于匹配请求中的列和值。**顺序敏感**。

为了模拟返回，可以通过`flare::testing::Return`以如下方式之一从`WillRepeatedly(...)`中返回：

- `flare::testing::Return(const flare::testing::CkvValues& values)`：返回成功，并将`values`作为返回值。
- `flare::testing::Return(const flare::testing::Errors& errors)`：返回失败，`errors`指定了针对各个key/列操作的结果。
- `flare::testing::Return(int status)`：返回失败，所有的key/列的操作都会以`status`错误结束。

```cpp
TEST(CkvMock, Legacy) {
  CkvChannel channel;
  FLARE_CHECK(channel.Open("mock://whatever-it-wants-to-be."));
  CkvClient client1(&channel, CkvClient::Options{.bid = 12345});

  FLARE_EXPECT_CKV_GET_KEYS(12345, CkvKeysEqual({"a", "b"}))
      .WillRepeatedly(testing::Return(CkvValues{"va", "vb"}));

  auto result1 = client1.GetKeys({"a", "b"});

  ASSERT_EQ(2, result1.size());
  ASSERT_TRUE(result1[0]);
  ASSERT_TRUE(result1[1]);
  EXPECT_EQ("va", *result1[0]);
  EXPECT_EQ("vb", *result1[1]);
}
```

## TDBank Mock

**为了能够在单测中截获TDBank调用，请求TDBank时需要使用`mock://...`方式的URI来指定`bid`/`tid`。**

我们提供了[`FLARE_EXPECT_TDBANK_WRITE`](../testing/tdbank_mock.h)用于解决单测对TDBank的依赖。这个宏接受三个参数，分别用于匹配`bid`、`tid`、一系列消息（`std::vector<std::string>`类型）。

为了模拟返回，可以通过`flare::testing::Return`返回`bool`。

为了手动处理TDBank写操作，可以通过`.WillXxx(flare::testing::HandleTdbankWrite(...))`接管。

```cpp
TEST(Tdbank, All) {
  bool called = false;

  FLARE_EXPECT_TDBANK_WRITE("123", "456", ::testing::_)
      .WillRepeatedly(
          HandleTdbankWrite([&](auto&& tid, auto&& bid, auto&& msg) {
            EXPECT_EQ("123", tid);
            EXPECT_EQ("456", bid);
            EXPECT_EQ("my fancy message", msg.front());

            called = true;
            return false;
          }));

  TdbankClient client("mock://123-456");
  EXPECT_FALSE(client.Write("my fancy message", 1s));
  EXPECT_TRUE(called);
}
```

## Redis Mock

**为了能够在单测中截获Redis调用，请求Redis时需要使用`mock://...`方式的URI来指定服务器地址。**

我们提供了[`FLARE_EXPECT_REDIS_COMMAND`](../testing/redis_mock.h)用于解决单测对Redis的依赖。这个宏接受一个参数，用于匹配客户端发出的Redis命令：

- `flare::testing::RedisCommandEq(...)`：用于精确匹配客户端发出的Redis命令。
- `flare::testing::RedisCommandOpEq(...)`：用于匹配客户端发出的Redis命令的操作码（`SET`、`GET`等）。
- `flare::testing::RedisCommandUserMatch(...)`：调用用户提供的回调来匹配请求。
- `::testing::_`可以用来匹配任意命令。

为了模拟返回，可以通过`flare::testing::Return`返回`flare::RedisCommand`。

```cpp
TEST(RedisMock, All) {
  RedisChannel channel;
  FLARE_CHECK(channel.Open("mock://whatever-it-wants-to-be."));
  RedisClient client(&channel);

  FLARE_EXPECT_REDIS_COMMAND(RedisCommandEq(RedisCommand("GET", "x")))
      .WillRepeatedly(Return(RedisString("str")));

  auto result = client.Execute(RedisCommand("GET", "x"), 1s);
  ASSERT_TRUE(result.is<RedisString>());
  EXPECT_EQ("str", *result.as<RedisString>());

  FLARE_EXPECT_REDIS_COMMAND(_).WillRepeatedly(Return(RedisNull()));

  EXPECT_TRUE(
      client.Execute(RedisCommand("GET", "not existing"), 1s).is<RedisNull>());
}
```

## COS Mock

**为了能够在单测中截获COS调用，请求COS时需要使用`mock://...`方式的URI来指定服务器地址。**

我们提供了[`FLARE_EXPECT_COS_OP`](../testing/cos_mock.h)用于解决单测对COS的依赖。这个宏接受一个参数，用于匹配客户端执行的COS的`操作类型`。

`操作类型`可以通过请求参数的类型来确定，如`flare::CosGetObjectRequest`对应的`操作类型`则是`GetObject`。

可以通过如下某种方式模拟响应：

- `flare::testing::Return(flare::Status)`：返回一个失败的错误信息。
- `flare::testing::HandleCosOp(F&& handler)`：对匹配了这一`FLARE_EXPECT_COS_OP`的COS相关调用均会转发至`handler`。`handler`应当有如下原型之一：
  - `flare::Status Handler(const flare::CosXxxRequest& request, flare::CosXxxResult* result)`：`Xxx`取决于`操作类型`。这一方法可以修改`result`，并根据需要返回一个成功或失败的状态码。
  - `flare::Status Handler(const flare::CosXxxRequest& request, flare::CosXxxResult* result, const flare::cos::CosTask::Options& options)`：同上，但是会通过`options`提供一些额外信息。

```cpp
TEST(CosMock, HandleCosOp2) {
  CosClient client;
  ASSERT_TRUE(client.Open("mock://...", {}));
  FLARE_EXPECT_COS_OP(GetObject).WillRepeatedly(HandleCosOp(
      [](const CosGetObjectRequest& req, CosGetObjectResult* result) {
        result->bytes = CreateBufferSlow("something");
        return Status();
      }));

  auto result = client.Execute(CosGetObjectRequest());
  ASSERT_TRUE(result);
  EXPECT_EQ("something", FlattenSlow(result->bytes));
}
```

## Mock非虚函数

对于某些无法使用gmock的场景，我们提供了[`FLARE_EXPECT_HOOKED_CALL`](../testing/hooking_mock.h)来进行测试。

需要注意的是，这种mock方式依赖于[Hook](https://en.wikipedia.org/wiki/Hooking#API/function_hooking/interception_using_JMP_instruction_aka_splicing)。这是一种很可能涉及到[UB](https://en.cppreference.com/w/cpp/language/ub)的解决方案，因此**除非必须，我们通常不推荐使用这一方法。**

受限于技术制约，在包括但不限于以下场景下，无法使用`FLARE_EXPECT_HOOKED_CALL`：

- 需要Mock的方法是虚方法。这种情况下无法取得函数的实际地址。另外这种情况下请优先考虑使用gmock自带的`MOCK_METHOD`完成。
- 需要Mock的方法被内联。这种情况下取得的函数地址可能只是一个“副本”，实际这一方法被调用的时候已经被内联了不会调用到这一副本。
- 函数体太短。对于非常短的函数体，函数体本身的长度可能不足以支撑我们需要生成的转跳指令的长度，这种情况下`FLARE_EXPECT_HOOKED_CALL`可能导致崩溃。
- 其他可能的编译器优化。如对于标记为[`[[noreturn]]`](https://en.cppreference.com/w/cpp/language/attributes/noreturn)的代码，编译器可能会在调用完后直接生成[`__builtin_unreachable()`](https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html#index-_005f_005fbuiltin_005funreachable)导致在mock方法中返回后崩溃。

*我们支持被重载的方法进行mock，但是此时[获取函数地址需要一定的技巧](https://stackoverflow.com/questions/45146522/c-address-of-overloaded-function)。*

这一宏可以通过`FLARE_EXPECT_HOOKED_CALL(AddressOfMethod, Matchers...)`的形式使用，如：

```cpp
volatile int xx = 0;

[[gnu::noinline]] void FancyNonVirtualMethod(int x) { xx = x; }

TEST(HookingMock, All) {
  std::vector<int> v;
  FLARE_EXPECT_HOOKED_CALL(FancyNonVirtualMethod, ::testing::_)
      .WillRepeatedly([&](int x) { v.push_back(x); });
  ASSERT_TRUE(v.empty());
  for (int i = 0; i != 12345; ++i) {
    FancyNonVirtualMethod(i);
    ASSERT_EQ(i, v.back());
  }
}
```

*这儿的示例是一个文件内部定义的非虚函数，实际上对于文件外的、或第三方库的方法，只要可以获取其地址，同样可以通过这种方式mock。*

## 部分常用工具

这儿可能并不是一个完整的列表，完整的列表请参考[源代码目录](../testing)。

- [获取一个可以被UT内的测试服务用来监听的测试端口/地址](../testing/endpoint.h)
- [访问`RpcController`内部部分私有方法](../testing/rpc_controller.h)
- [访问`HbaseController`内部部分私有方法](../testing/hbase_controller.h)

---
[返回目录](README.md)
