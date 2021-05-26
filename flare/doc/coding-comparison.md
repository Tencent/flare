# 编码比较

我们简单对比了flare和现有框架的在RPC方面编码的开发及阅读成本。

## 普通RPC

### 服务端

现有框架（使用线程池执行逻辑避免阻塞IO）：

```cpp
class EchoServiceImpl : public EchoService {
 public:
  void Echo(
      google::protobuf::RpcController* controller,
      const EchoRequest* request,
      EchoResponse* response,
      google::protobuf::Closure* done) override {
    auto cb = [request, response, done] {
      ++s_count;
      response->set_response(std::string(request->response_length(), 'A'));
      done->Run();
    };
    tp->PushTask(std::move(cb));
  }

  EchoServiceImpl() {
    tp->Start();
  }

 private:
  std::shared_ptr<ThreadPool> tp = ThreadPool::Create("worker pool", 4);
};
```

flare（回调不会阻塞IO无需线程池）：

```cpp
class EchoServiceImpl : public SyncEchoService {
 public:
  void Echo(const EchoRequest& request, EchoResponse* response,
            RpcServerController* controller) {
    response->set_response(std::string(request->response_length(), 'A'));
  }
};
```

### 客户端

需要注意的是，此处现有RPC框架的示例使用同步方式调用，可能阻塞[pthread](https://en.wikipedia.org/wiki/POSIX_Threads)对服务吞吐造成明显影响。如果需要异步调用需要业务代码自行保存相关状态（临时变量等）及维护其生命周期。

flare版本由于只会阻塞[fiber](fiber.md)而不会阻塞底层pthread，因此吞吐不受影响。

现有框架：

```cpp
void StreamCall() {
  RpcChannel channel;
  RpcController controller;
  EchoRequest request;
  EchoResponse response;
  
  CHECK(RpcClient::DefaultInstance()->OpenChannel(/* address */, &channel));
  EchoService_Stub stub(&channel);
  stub.Echo(&request, &response, &controller);  // MAY BLOCK PTHREAD.
  if (!controller.Failed()) {
    LOG(INFO) << response.body();
  } else {
    LOG(ERROR) << controller.ErrorText();
  }
}
```

flare：

```cpp
void StreamCall() {
  RpcChannel channel;
  RpcClientController controller;
  EchoRequest request;

  FLARE_CHECK(channel.Open(/* address */));
  EchoService_Stub stub(&channel);
  auto resp = stub.Echo(request, &controller);  // WON'T BLOCK PTHREAD.
  if (resp) {
    FLARE_LOG_INFO("{}", resp->body());
  } else {
    FLARE_LOG_ERROR("{}", resp->Error().Description());
  }
}
```

## 流式RPC

现有框架只支持服务端流式返回，不支持客户端流式请求或双向流，因此只对比服务端流式返回的场景。

flare对客户端流式请求或双向流提供了与服务端流式返回类似的简易接口。

### 传统服务端

现有框架（使用线程池执行逻辑避免阻塞IO）：

```cpp
class EchoResponseStream :
    public RpcServerResponseStream<EchoResponse, EchoResponseStream> {
 public:
  EchoResponseStream(RpcController* controller, shared_ptr<ThreadPool> thread_pool)
    : num_(0), closed_(false), thread_pool_(thread_pool) {
  }

  void DoWorkTask(std::weak_ptr<EchoResponseStream> weak_this) {
    std::shared_ptr<EchoResponseStream> ptr = weak_this.lock();
    if (!ptr)
      return;
    DoWork();
  }

  void DoWork() {
    MutexLocker locker(&mutex_);
    if (closed_) {
      WriteComplete();
      return;
    }
    EchoResponse response;
    response.set_response(std::string("A"));
    Write(&response);
    s_count++;
  }

  void Next() override {
    {
      MutexLocker locker(&mutex_);
      if (num_++ >= FLAGS_stream_response_num) {
        if (!closed_) {
          WriteComplete();
          closed_ = true;
        }
        return;
      }
    }
    if (thread_pool_.get()) {
      thread_pool_->PushTask(
          NewCallback(this, &EchoResponseStream::DoWorkTask,
          std::weak_ptr<EchoResponseStream>(SharedFromThis())));
    } else {
      DoWork();
    }
  }

  void Close() override {
    MutexLocker locker(&mutex_);
    if (closed_)
      return;
    closed_ = true;
  }

 private:
  int num_;
  bool closed_;
  mutable Mutex mutex_;
  shared_ptr<ThreadPool> thread_pool_;
};

class EchoServiceImpl : public EchoService {
 public:
  explicit EchoServiceImpl() :
      thread_pool_(thread_pool) {}
  void StreamEcho(
      google::protobuf::RpcController* controller,
      const EchoRequest* request,
      EchoResponse* response,
      google::protobuf::Closure* done) override {
    auto gdt_controller = (gdt::RpcController*)controller;
    auto stream = new EchoResponseStream(gdt_controller, thread_pool_);
    gdt_controller->SetRpcServerResponseStream(stream);
  }

 private:
  shared_ptr<ThreadPool> thread_pool_;
};
```

flare（回调不会阻塞IO无需线程池）：

```cpp
class EchoServiceImpl : public SyncEchoService {
 public:
  void StreamEcho(const EchoRequest& request, StreamWriter<EchoResponse> writer,
                  RpcServerController* controller) override {
    for (int i = 0; i != FLAGS_stream_response_num; ++i) {
      EchoResponse response;
      response.set_response(std::string("A"));
      writer.Write(std::move(response));
    }
    writer.Close();
  }
};
```

### 传统客户端

此处现有框架同样采用同步方式调用，因此会对吞吐造成明显影响。异步方式需要自行保存状态等。

现有框架：

```cpp
void StreamCall() {
  RpcChannel channel;
  RpcController controller;
  EchoRequest request;
  EchoResponse response;

  CHECK(RpcClient::DefaultInstance()->OpenChannel(/* address */, &channel));
  EchoService_Stub stub(&channel);
  RpcResponseStream stream(&stub, &EchoService::Stub::StreamEcho, &controller);

  if (!stream.StartFlow(&request, &response, [&] () {
    if (stream.IsComplete()) {
      if (controller.Failed()) {
        LOG(ERROR) << controller.ErrorText();
      } else {
        LOG(INFO) << response.body();  // Variable `response` is reused by each
                                       // response.
      }
      return;
    }
  })) {
    LOG(ERROR) << controller.ErrorText();
    return;
  }

  // We either have to wait for call to complete, or allocate objects above on
  // heap.
  while (!stream.IsComplete()) {
    usleep(100);
  }
}
```

flare：

```cpp
void StreamCall() {
  RpcChannel channel;
  RpcClientController controller;
  EchoRequest request;

  FLARE_CHECK(channel.Open(/* address */));
  EchoService_Stub stub(&channel);
  auto reader = stub.StreamEcho(request, &controller);
  while (auto v = read.Read()) {
    FLARE_LOG_INFO("{}", v->body());
  }
  if (controller.Failed()) {
    FLARE_LOG_ERROR("{}", controller.ErrorText());
  }
}
```

---
[返回目录](README.md)
