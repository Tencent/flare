// Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of the
// License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include "flare/rpc/server.h"

#include <sys/signal.h>

#include <thread>

#include "googletest/gtest/gtest.h"
#include "jsoncpp/json.h"

#include "flare/fiber/async.h"
#include "flare/fiber/latch.h"
#include "flare/fiber/this_fiber.h"
#include "flare/net/http/http_client.h"
#include "flare/rpc/http_handler.h"
#include "flare/rpc/internal/stream_call_gate.h"
#include "flare/rpc/protocol/stream_service.h"
#include "flare/rpc/rpc_channel.h"
#include "flare/rpc/rpc_client_controller.h"
#include "flare/testing/echo_service.flare.pb.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"

using namespace std::literals;
using flare::rpc::internal::StreamCallGate;

DECLARE_int32(flare_rpc_server_max_ongoing_calls);
DECLARE_int32(flare_rpc_server_remove_idle_connection_interval);
DECLARE_int32(flare_rpc_server_connection_max_idle);
DECLARE_int32(flare_rpc_server_max_connections);
DECLARE_string(flare_io_cap_rx_bandwidth);
DECLARE_string(flare_io_cap_tx_bandwidth);

namespace flare {

std::atomic<std::uint64_t> cid = 1;

class EchoMessage : public Message {
 public:
  EchoMessage() = default;
  explicit EchoMessage(std::uint64_t cid) : cid_(cid) {}
  std::uint64_t GetCorrelationId() const noexcept override { return cid_; }
  Type GetType() const noexcept override { return Type::Single; }

 private:
  std::uint64_t cid_ = ++cid;
};

class ErrorMessageFactory : public MessageFactory {
 public:
  std::unique_ptr<Message> Create(Type type, std::uint64_t correlation_id,
                                  bool stream) const override {
    // Correlation ID `0` is never used, so it's treated as an error by the
    // client side.
    return std::make_unique<EchoMessage>(0);
  }
};

class SleepyEchoService : public StreamService {
 public:
  const experimental::Uuid& GetUuid() const noexcept override {
    static constexpr experimental::Uuid kUuid(
        "A810E368-9990-49FF-A1C1-F75D58E4C5B5");
    return kUuid;
  }
  bool Inspect(const Message&, const flare::Controller&,
               InspectionResult*) override {
    return true;
  }
  bool ExtractCall(const std::string& serialized,
                   const std::vector<std::string>& serialized_pkt_ctxs,
                   ExtractedCall* extracted) override {
    return false;
  }
  ProcessingStatus FastCall(
      std::unique_ptr<Message>* message,
      const FunctionView<std::size_t(const Message&)>& writer,
      Context* context) override {
    this_fiber::SleepFor(2s);
    writer(EchoMessage((*message)->GetCorrelationId()));
    return ProcessingStatus::Processed;
  }
  ProcessingStatus StreamCall(
      AsyncStreamReader<std::unique_ptr<Message>>* input_stream,
      AsyncStreamWriter<std::unique_ptr<Message>>* output_stream,
      Context* context) override {
    CHECK(0);
  }
  void Stop() override {}
  void Join() override {}
};

class EchoService : public StreamService {
 public:
  const experimental::Uuid& GetUuid() const noexcept override {
    static constexpr experimental::Uuid kUuid(
        "A810E368-9990-49FF-A1C1-F75D58E4C5B5");
    return kUuid;
  }
  bool Inspect(const Message&, const Controller&, InspectionResult*) override {
    return true;
  }
  bool ExtractCall(const std::string& serialized,
                   const std::vector<std::string>& serialized_pkt_ctxs,
                   ExtractedCall* extracted) override {
    return false;
  }
  ProcessingStatus FastCall(
      std::unique_ptr<Message>* message,
      const FunctionView<std::size_t(const Message&)>& writer,
      Context* context) override {
    writer(EchoMessage((*message)->GetCorrelationId()));
    return ProcessingStatus::Processed;
  }
  ProcessingStatus StreamCall(
      AsyncStreamReader<std::unique_ptr<Message>>* input_stream,
      AsyncStreamWriter<std::unique_ptr<Message>>* output_stream,
      Context* context) override {
    CHECK(0);
  }
  void Stop() override {}
  void Join() override {}
};

class EchoProtocol : public StreamProtocol {
 public:
  EchoProtocol() = default;
  explicit EchoProtocol(bool f) : create_null_msg_(f) {}

  const Characteristics& GetCharacteristics() const override {
    static const Characteristics cs = {.name = "EchoProtocol"};
    return cs;
  }

  const MessageFactory* GetMessageFactory() const override {
    if (create_null_msg_) {
      return MessageFactory::null_factory;
    } else {
      static ErrorMessageFactory emf;
      return &emf;
    }
  }

  const ControllerFactory* GetControllerFactory() const override {
    return ControllerFactory::null_factory;
  }

  MessageCutStatus TryCutMessage(NoncontiguousBuffer* buffer,
                                 std::unique_ptr<Message>* message) override {
    CHECK(!buffer->Empty());
    std::uint64_t cid;
    if (buffer->ByteSize() < sizeof(cid)) {
      return MessageCutStatus::NeedMore;
    }
    FlattenToSlow(*buffer, &cid, sizeof(cid));
    buffer->Skip(sizeof(cid));
    *message = std::make_unique<EchoMessage>(cid);
    return MessageCutStatus::Cut;
  }

  bool TryParse(std::unique_ptr<Message>* message,
                Controller* controller) override {
    return true;
  }

  void WriteMessage(const Message& message, NoncontiguousBuffer* buffer,
                    Controller* controller) override {
    auto cid = message.GetCorrelationId();
    buffer->Append(CreateBufferSlow(&cid, sizeof(cid)));
  }

 private:
  bool create_null_msg_ = false;
};

class DummyProtoEcho : public testing::SyncEchoService {
 public:
  void Echo(const testing::EchoRequest& request,
            testing::EchoResponse* response,
            RpcServerController* controller) override {
    // NOTHING.
  }
};

TEST(Server, OverloadTest) {
  auto ep = testing::PickAvailableEndpoint();
  Server server{Server::Options{.max_concurrent_requests = 100}};

  server.AddProtocol(&std::make_unique<EchoProtocol>);
  server.AddNativeService(std::make_unique<SleepyEchoService>());
  server.ListenOn(ep);
  server.Start();

  auto gate = MakeRefCounted<rpc::internal::StreamCallGate>();
  StreamCallGate::Options opts = {.protocol = std::make_unique<EchoProtocol>(),
                                  .maximum_packet_size = 65536};
  gate->Open(ep, std::move(opts));
  CHECK(gate->Healthy());
  std::atomic<std::size_t> succeeded = 0;
  std::atomic<std::size_t> done_count = 0;
  for (int i = 0; i != 10000; ++i) {
    auto call_args = object_pool::Get<StreamCallGate::FastCallArgs>();
    call_args->completion = [&](auto, auto&& p, auto) {
      succeeded += !!p;
      ++done_count;
    };
    gate->FastCall(EchoMessage(), std::move(call_args), ReadSteadyClock() + 3s);
  }
  while (done_count != 10000) {
  }
  // Others are dropped.
  ASSERT_EQ(100, succeeded.load());
  gate->Stop();
  gate->Join();

  server.Stop();
  server.Join();
}

TEST(Server, OverloadTestNoCreateSpecialMessage) {
  auto ep = testing::PickAvailableEndpoint();
  Server server{Server::Options{.max_concurrent_requests = 100}};

  server.AddProtocol([] { return std::make_unique<EchoProtocol>(true); });
  server.AddNativeService(std::make_unique<SleepyEchoService>());
  server.ListenOn(ep);
  server.Start();

  auto gate = MakeRefCounted<rpc::internal::StreamCallGate>();
  StreamCallGate::Options opts = {.protocol = std::make_unique<EchoProtocol>(),
                                  .maximum_packet_size = 65536};
  gate->Open(ep, std::move(opts));
  CHECK(gate->Healthy());
  std::atomic<std::size_t> succeeded = 0;
  std::atomic<std::size_t> done_count = 0;
  for (int i = 0; i != 10000; ++i) {
    auto call_args = object_pool::Get<StreamCallGate::FastCallArgs>();
    call_args->completion = [&](auto, auto&& p, auto) {
      succeeded += !!p;
      ++done_count;
    };
    gate->FastCall(EchoMessage(), std::move(call_args), ReadSteadyClock() + 3s);
  }
  while (done_count != 10000) {
  }
  // Others are dropped.
  ASSERT_EQ(100, succeeded.load());
  gate->Stop();
  gate->Join();

  server.Stop();
  server.Join();
}

TEST(Server, BuiltinHttpService) {
  auto ep = testing::PickAvailableEndpoint();
  Server server;
  server.ListenOn(ep);
  server.Start();

  HttpClient client;
  auto resp = client.Get("http://" + ep.ToString() + "/inspect/version");
  ASSERT_TRUE(resp);
  ASSERT_NE(std::string::npos, resp->body()->find("BuildTime"));

  server.Stop();
  server.Join();
}

TEST(Server, NoBuiltinHttpService) {
  auto ep = testing::PickAvailableEndpoint();
  Server server(Server::Options{.no_builtin_pages = true});
  // We need at least one service to be available, otherwise `server` has
  // nothing to serve and will crash the UT.
  server.AddHttpHandler("/path/to/something",
                        NewHttpGetHandler([](auto&&, auto&&, auto&&) {}));
  server.ListenOn(ep);
  server.Start();

  HttpClient client;
  auto resp = client.Get("http://" + ep.ToString() + "/inspect/version");
  ASSERT_TRUE(resp);
  EXPECT_EQ(HttpStatus::NotFound, resp->status());
}

TEST(Server, NoBuiltinHttpServiceViaFlags) {
  google::FlagSaver _;
  FLAGS_flare_rpc_server_no_builtin_pages = true;
  auto ep = testing::PickAvailableEndpoint();
  Server server;
  // We need at least one service to be available, otherwise `server` has
  // nothing to serve and will crash the UT.
  server.AddHttpHandler("/path/to/something",
                        NewHttpGetHandler([](auto&&, auto&&, auto&&) {}));
  server.ListenOn(ep);
  server.Start();

  HttpClient client;
  auto resp = client.Get("http://" + ep.ToString() + "/inspect/version");
  ASSERT_TRUE(resp);
  EXPECT_EQ(HttpStatus::NotFound, resp->status());
}

TEST(Server, RemoveIdleConnection) {
  google::FlagSaver fs;
  FLAGS_flare_rpc_server_connection_max_idle = 1;
  FLAGS_flare_rpc_server_remove_idle_connection_interval = 1;

  auto ep = testing::PickAvailableEndpoint();
  Server server;

  server.AddProtocol(&std::make_unique<EchoProtocol>);
  server.AddNativeService(std::make_unique<EchoService>());
  server.ListenOn(ep);
  server.Start();

  auto gate = MakeRefCounted<rpc::internal::StreamCallGate>();
  StreamCallGate::Options opts = {.protocol = std::make_unique<EchoProtocol>(),
                                  .maximum_packet_size = 65536};
  gate->Open(ep, std::move(opts));
  CHECK(gate->Healthy());
  std::atomic<bool> done_count{};
  auto call_args = object_pool::Get<StreamCallGate::FastCallArgs>();
  call_args->completion = [&](auto, auto&& p, auto) { done_count = true; };
  gate->FastCall(EchoMessage(), std::move(call_args), ReadSteadyClock() + 3s);
  while (!done_count) {
  }
  // Don't test `alive_conns_` here, or TSan would complain about data race.
  ASSERT_EQ(1, server.alive_conns_);
  std::this_thread::sleep_for(10ms);
  ASSERT_EQ(1, server.alive_conns_);
  std::this_thread::sleep_for(3s);
  ASSERT_EQ(0, server.alive_conns_);
  gate->Stop();
  gate->Join();
  ASSERT_EQ(0, server.conns_.size());

  server.Stop();
  server.Join();
}

RefPtr<rpc::internal::StreamCallGate> MakeCallTo(const Endpoint& ep) {
  auto ptr = MakeRefCounted<rpc::internal::StreamCallGate>();
  StreamCallGate::Options opts = {.protocol = std::make_unique<EchoProtocol>(),
                                  .maximum_packet_size = 65536};
  ptr->Open(ep, std::move(opts));
  if (!ptr->Healthy()) {
    return nullptr;
  }

  fiber::Latch l(1);
  std::atomic<bool> result{};
  auto call_args = object_pool::Get<StreamCallGate::FastCallArgs>();
  call_args->completion = [&](auto, auto&& p, auto) {
    result = !!p;
    l.count_down();
  };
  ptr->FastCall(EchoMessage(), std::move(call_args), ReadSteadyClock() + 3s);
  l.wait();
  if (result) {
    return ptr;
  }
  ptr->Stop();
  ptr->Join();
  return nullptr;
}

TEST(Server, TooManyConnections) {
  auto ep = testing::PickAvailableEndpoint();
  Server server{Server::Options{.max_concurrent_connections = 10}};

  server.AddProtocol(&std::make_unique<EchoProtocol>);
  server.AddNativeService(std::make_unique<EchoService>());
  server.ListenOn(ep);
  server.Start();

  std::vector<RefPtr<rpc::internal::StreamCallGate>> gates;
  for (int i = 0; i != 10; ++i) {
    auto p = MakeCallTo(ep);
    EXPECT_TRUE(p);
    gates.push_back(p);
  }
  std::vector<Future<>> vfs;
  for (int i = 0; i != 1000; ++i) {
    auto f = fiber::Async([&] {
      for (int j = 0; j != 10; ++j) {
        auto p = MakeCallTo(ep);
        EXPECT_FALSE(p);  // New connections are rejected.
        if (p) {
          p->Stop();
          p->Join();
        }
      }
    });
    vfs.push_back(std::move(f));
  }
  fiber::BlockingGet(WhenAll(&vfs));
  for (auto&& e : gates) {
    e->Stop();
    e->Join();
  }

  server.Stop();
  server.Join();
}

TEST(Server, QueueingDelayReject) {
  auto ep = testing::PickAvailableEndpoint();
  Server server{
      Server::Options{.max_request_queueing_delay =
                          1ns /* Almost guaranteed to be in effect.*/}};

  server.ListenOn(ep);
  server.AddProtocol("flare");
  server.AddService(std::make_unique<DummyProtoEcho>());
  server.Start();

  testing::EchoService_SyncStub stub("flare://" + ep.ToString());
  RpcClientController ctlr;
  EXPECT_EQ(rpc::STATUS_OVERLOADED,
            stub.Echo(testing::EchoRequest(), &ctlr).error().code());
}

TEST(Server, QueueingDelaySafe) {
  auto ep = testing::PickAvailableEndpoint();
  Server server{Server::Options{.max_request_queueing_delay = 1s /* Safe? */}};

  server.ListenOn(ep);
  server.AddProtocol("flare");
  server.AddService(std::make_unique<DummyProtoEcho>());
  server.Start();

  testing::EchoService_SyncStub stub("flare://" + ep.ToString());
  RpcClientController ctlr;
  EXPECT_TRUE(stub.Echo(testing::EchoRequest(), &ctlr));
}

TEST(Server, DenyingConnections) {
  auto ep = testing::PickAvailableEndpoint();
  Server server{Server::Options{.conn_filter = [](auto&&) { return false; }}};

  server.ListenOn(ep);
  server.AddProtocol("flare");
  server.AddService(std::make_unique<DummyProtoEcho>());
  server.Start();

  testing::EchoService_SyncStub stub("flare://" + ep.ToString());
  RpcClientController ctlr;
  EXPECT_FALSE(stub.Echo(testing::EchoRequest(), &ctlr));
  EXPECT_EQ(rpc::STATUS_IO_ERROR, ctlr.ErrorCode());
}

}  // namespace flare

FLARE_TEST_MAIN
