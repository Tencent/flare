# Promise/Future模型

多个 Fiber 之间是并行的，但是一个 Fiber 内的代码是顺序执行的。现实中很多服务往往需要调用多个其他后端服务，如果这些被调服务之间没有顺序依赖，就可以可以创建多个 Fiber 来异步并发调用，从而减少不必要的等待，提高性能。

为了获得异步操作的结果，我们引入了 [Promise/Future](https://zh.wikipedia.org/wiki/Future%E4%B8%8Epromise) 作为协作机制。

关于`Promise<Ts...>` / `Future<Ts...>`的使用可以参考其[接口文档](../base/future/README.md)，此处不再赘述。

*此处使用`Ts...`的原因是我们的Promise/Future库可以传递0个或超过一个元素，具体参见接口文档。*

此文档主要说明在Flare环境中这一模型一些需要注意的问题。

首先，与大多数同时支持协程、Future模型的RPC框架需要在两种模型二选一不同，**Flare环境中，[Fiber](fiber.md)和Promise/Future*不是*二选一的关系，我们允许在Fiber中任意（包括阻塞等待）的使用Future**。

事实上，在我们看来，Future作为一种异步的消息传递机制，其不应当对选择其他编程模型本身造成限制。如pthread环境中，既可以通过发起多个异步操作然后阻塞等待所有操作完成然后继续*同步*执行，也可以通过一系列continuation-chain（`Future<...>::Then`）实现*异步*编码。

## 示例

我们首先给出一个简单的在Fiber环境下执行异步操作并等待返回的示例代码。

这个示例假设存在一个异步的HTTP库（这个库有自己的IO事件循环等等），允许我们传入一个需要GET的URI和回调，并在收到结果时在*HTTP库自己的线程池中*调用我们的回调。

这个示例主要说明了如下几点：

- 我们可以通过`Promise<Ts...>` / `Future<Ts...>`同时传递多个值（`status` + `body`）。

  `Future<Ts...>::Then`接受的continuation可以直接接受多个参数，不需要通过`std::tuple<...>`二次包装。

- 我们可以通过`Promise<Ts...>` / `Future<Ts...>`**从pthread环境传递数据到fiber环境中**。

  此处调用`Promise<Ts...>::SetValue`的代码（即传给HTTP库的回调）位于HTTP库自己的pthread线程池（非Flare环境），而`flare::fiber::BlockingGet(...)`的调用时机为Flare环境中的某个fiber中。

- 在Fiber中阻塞等待`Future<Ts...>`**不会阻塞底层pthread**，因此**没有性能损耗**（除了`Future<Ts...>`本身固有的移动对象等开销外）。

  因此我们可以将异步方法转换成*只阻塞Fiber*的同步方法，并且不影响我们实现的函数本身（对外）的同步接口（即对外不需要返回`Future<Ts...>`）。

  *注意这儿需要使用`flare::fiber::BlockingGet(...)`，如果使用`flare::BlockingGet(...)`（内部通过pthread的同步原语实现）则会阻塞底层pthread。如果使用Flare的RPC框架，则通常均应当使用前者。*

这种设计亦可以用于将多数其他异步客户端以较小的成本转换为Fiber环境下可用的同步版本（而不阻塞底层pthread）。

```cpp
// For illustration purpose only.
//
// You normally shouldn't do this, consider using `flare::HttpClient` instead.
//
// Callback `cb` is called in HTTP library's own (pthread) thread pool.
void AsyncHttpGet(const std::string& uri, Function<void(int, std::string)> cb);

// Protocol Buffers service's method implementation.
//
// Run in fiber environment (i.e., any synchronous RPC won't block underlying
// pthread, as obvious).
void EchoServiceImpl::Echo(...) {
  flare::Promise<int, std::string> p;
  auto future = p.GetFuture();

  // Call asynchronous method (`AsyncHttpGet(...)`).
  auto http_cb = [p = std::move(p)] (int status, std::string body) mutable {
    p.SetValue(status, std::move(body));
  };
  AsyncHttpGet(req.uri(), std::move(http_cb));

  // Block this fiber (but NOT the underlying pthread) until the result is
  // available.
  //
  // `BlockingGet(...)` returns `std::tuple` if (and only if) there are more
  //  than one types in `Future<Ts...>`, here we're using structured binding to
  // expand the tuple.
  auto&& [status, body] = flare::fiber::BlockingGet(&future);
  if (status != 200) {
    FLARE_LOG_WARNING("HTTP request failed with status {}.", status);
  } else {
    FLARE_LOG_INFO("Received HTTP response: {}", body);
  }
}
```

## Future库常见的设计错误

往往`Future<Ts...>`的设计者会犯的错误在于，大多数的`Future<Ts...>`同时肩负了两种*不同*的职责：

- 如果提供`Future<Ts...>::Get()`：那么`Future<Ts...>`应当是一种阻塞等待异步操作返回的*同步*（动词）工具。

  这一种设计的例子包括但不限于pre-C++20（未合入[The C++ Extensions for Concurrency, ISO/IEC TS 19571:2016](https://en.cppreference.com/w/cpp/experimental/concurrency)的版本，其实截止行文C++20也没合入这个TS）中的[`std::future<T>`](https://en.cppreference.com/w/cpp/thread/future)）

- 如果提供`Future<Ts...>::Then(...)`：那么`Future<Ts...>`应当是一种*执行控制流描述*工具。

  这一设计的例子包括但不限于[`std::experimental::future<T>`](https://en.cppreference.com/w/cpp/experimental/future)、[`boost::future<T>`](https://www.boost.org/doc/libs/1_72_0/libs/fiber/doc/html/fiber/synchronization/futures/future.html)、[`stlab::future<T>`](https://stlab.cc/libraries/concurrency/future/future/)。（其中前两者也实现了`future<T>::get()`。这种设计的问题见后文。）

这样的设计一方面不必要的增加了`Future<Ts...>`的学习成本（身兼数职），另外一方面有如下问题：

- 在C++11引入了移动语义之后，事实上`Future<Ts...>::Get()`的行为已经变的有些反直觉。

  这个问题主要在于有了移动语义之后，通常会需要`Future<Ts...>::Get()`返回`T`（将之*移动*到返回值中，否则无法支持move-only的类型），这会使得`Get()`不是幂等的，与通常所说的`Get()`操作不符。

  有部分库通过返回`T&`来弥补这一问题，这从语义上将*从`Promise<Ts...>`传递结果到`Future<Ts...>`*变成了*在`Promise<Ts...>`和`Future<Ts...>`之间共享数据*。

- 通过定义`Future<Ts...>::Get()`，实际上将`Future<Ts...>`绑定到了特定的线程模型中。

  **这一条实际上也是部分RPC框架要求协程、Future二选一的原因。**

  由于在协程中阻塞等待Future（即调用`Future<Ts...>::Get()`）会直接阻塞底层pthread，这会对整体的吞吐、响应速度等造成明显的影响，因此多数框架不推荐在协程中阻塞等待Future。

- `T`有两种出口：`Future<Ts...>::Get()`和`Future<Ts...>::Then(...)`的continuation。

  这也潜在的提供了在`Then(...)`之后企图使用`Get()`等待操作完成的误用的可能性。

## 合理的Future库设计

简单来说，我们认为：合理设计的**Future库不应当同时提供`Future<Ts...>::Get()`和`Future<Ts...>::Then(...)`**。

*如果提供`Future<Ts...>::Get()`，则是一种pthread间（或其他由实现决定的线程模型）（同步）传递信息的方式。否则则用于声明控制流。（见上文）*

进一步的，我们认同[Felix Petriconi在code::dive 2018上There is a Better Future](https://www.youtube.com/watch?v=WZdKFlH7qxo)所表达的意见，即：**`Future<Ts...>`本身不应当提供`Get()`，而应当作为独立方法（如`BlockingGet(Future<Ts...>*)`提供）。**

这样的设计实际上解决了很多问题：

- `Future<Ts...>`不再绑定到某种特定的线程模型。

  在我们的设计中，可以通过`flare::BlockingGet(Future<Ts...>*)`来阻塞底层pthread（通常用于非fiber环境）等待结果，也可以通过（fiber环境下）`flare::fiber::BlockingGet(Future<Ts...>*)`来阻塞fiber（但*不*阻塞底层pthread）来等待结果。

  *别担心，我们通过特定的trick（[`future/utils.h`文件尾部](../base/future/utils.h)）避免了符号寻找时ADL生效。因此unqualified `BlockingGet(...)`会编译错误，而不会直接默认使用`flare::BlockingGet(...)`。*

  **因此我们的fiber环境支持使用Future乃至可以阻塞等待Future且不造成（除Future本身固有的执行成本外的）性能影响。**

  *对于检索等需要同时请求多个下游的场景，可以通过发起一系列异步RPC后`flare::fiber::BlockingGet(WhenAll(...))`等待所有RPC返回，然后继续使用同步模型编码。既可以满足并发请求的需求又不需要将Promise/Future模型对外暴露（即无模型的传染性）。*

  事实上我们的设计更进一步，**不但允许pthread之间、或fiber之间通过`Future<Ts...>`传递数据，也支持在pthread和fiber之间传递数据**。

  如可以在pthread中调用`Promise<Ts...>::SetValue(...)`、在fiber中调用`flare::fiber::BlockingGet(...)`来实现从pthread向fiber传递数据。

  *从fiber向pthread传递数据只依赖pthread，因此各种`future<T>`均支持，没有太大特殊性。*

- 可以通过多种方式等待`Future<Ts...>`完成。

  包括但不限于`BlockingTryGet(Future<Ts...>*, Timeout)`这种支持超时等方式。

  *严格来说传统的给`Future<Ts...>`增加成员方法的实现也可以支持这种方式，如增加`Future<Ts...>::TryGet(...)`。当然这会更进一步的复杂化`Future<Ts...>`的接口。*

- `Future<Ts...>`不再身兼数职，本身类的设计变得精简且纯粹。

这种设计的最终效果见“示例”一节。

## 注意事项

我们尚未对我们的Future库专门的进行性能调优，但应当足以满足一般需求。

由于我们的编码规范，我们的Future设计并不支持异常（曾经支持过后来发现在continuation跑出异常时静默吞掉会容易隐藏BUG，因此又删掉了。目前代码可能还有一些看似“过度设计”的残留的痕迹，基本都是由于当初支持异常所引入的）。如果对异常的需求较大，后续我们可能通过增加GFlags的方式再次允许使用异常。

---
[返回目录](README.md)
