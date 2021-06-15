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

#include "flare/io/native/stream_connection.h"

#include <sys/signal.h>

#include <memory>
#include <string>
#include <thread>
#include <utility>

#include "gtest/gtest.h"

#include "flare/base/chrono.h"
#include "flare/base/string.h"
#include "flare/fiber/this_fiber.h"
#include "flare/io/event_loop.h"
#include "flare/io/native/acceptor.h"
#include "flare/io/util/rate_limiter.h"
#include "flare/io/util/socket.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"

using namespace std::literals;

namespace flare {

class ConnectionHandler : public StreamConnectionHandler {
 public:
  using Callback = Function<DataConsumptionStatus(NoncontiguousBuffer*)>;

  explicit ConnectionHandler(std::string name, Callback cb)
      : name_(std::move(name)), cb_(std::move(cb)) {}

  void OnAttach(StreamConnection*) override {}
  void OnDetach() override {}

  void OnWriteBufferEmpty() override {}
  void OnDataWritten(std::uintptr_t ctx) override {}

  DataConsumptionStatus OnDataArrival(NoncontiguousBuffer* buffer) override {
    return cb_(buffer);
  }

  void OnClose() override {}
  void OnError() override { CHECK(!"Unexpected."); }

 private:
  std::string name_;
  Callback cb_;
};

class ClosedConnectionHandler : public StreamConnectionHandler {
 public:
  void OnAttach(StreamConnection*) override {}
  void OnDetach() override {}
  void OnWriteBufferEmpty() override { CHECK(!"Unexpected."); }
  void OnDataWritten(std::uintptr_t ctx) override { CHECK(!"Unexpected."); }
  DataConsumptionStatus OnDataArrival(NoncontiguousBuffer* buffer) override {
    CHECK(!"Unexpected.");
  }
  void OnClose() override { ++closed; }
  void OnError() override { CHECK(!"Unexpected."); };

  std::atomic<int> closed = 0;
};

class ErrorConnectionHandler : public StreamConnectionHandler {
 public:
  void OnAttach(StreamConnection*) override {}
  void OnDetach() override {}
  void OnWriteBufferEmpty() override { CHECK(!"Unexpected."); }
  void OnDataWritten(std::uintptr_t ctx) override { CHECK(!"Unexpected."); }
  DataConsumptionStatus OnDataArrival(NoncontiguousBuffer* buffer) override {
    CHECK(!"Unexpected.");
  }
  void OnClose() override { CHECK(!"Unexpected."); }
  void OnError() override { ++err; };

  std::atomic<int> err = 0;
};

class NativeStreamConnectionTest : public ::testing::Test {
 public:
  void SetUp() override {
    conns_ = 0;
    // backlog must be large enough for connections below (made in burst) to
    // succeeded.
    auto listen_fd = io::util::CreateListener(addr_, kConnectAttempts);
    CHECK(listen_fd);
    NativeAcceptor::Options opts;
    // A simple echo server.
    opts.connection_handler = [&](Handle fd, const Endpoint& peer) {
      auto index = conns_++;
      if (!accept_conns) {
        std::cout << "Rejecting connection." << std::endl;
        // Refuse further connections.
        return;
      }
      CHECK_LT(index, kConnectAttempts);

      io::util::SetNonBlocking(fd.Get());
      io::util::SetCloseOnExec(fd.Get());
      NativeStreamConnection::Options opts;
      opts.read_buffer_size = 11111;
      opts.handler = std::make_unique<ConnectionHandler>(
          Format("server handler {}", index),
          [this, index](NoncontiguousBuffer* buffer) {
            server_conns_[index]->Write(std::move(*buffer), 0);
            return StreamConnectionHandler::DataConsumptionStatus::Ready;
          });
      server_conns_[index] = MakeRefCounted<NativeStreamConnection>(
          std::move(fd), std::move(opts));
      GetGlobalEventLoop(0, server_conns_[index]->fd())
          ->AttachDescriptor(server_conns_[index].Get());
      server_conns_[index]->StartHandshaking();
    };
    io::util::SetNonBlocking(listen_fd.Get());
    io::util::SetCloseOnExec(listen_fd.Get());
    auto fdv = listen_fd.Get();
    acceptor_ =
        MakeRefCounted<NativeAcceptor>(std::move(listen_fd), std::move(opts));
    GetGlobalEventLoop(0, fdv)->AttachDescriptor(acceptor_.Get());
  }

  void TearDown() override {
    acceptor_->Stop();
    acceptor_->Join();
    for (auto&& e : server_conns_) {
      if (e) {
        e->Stop();
        e->Join();
        e = nullptr;
      }
    }
  }

 protected:
  // Default of `net.core.somaxconn`.
  static constexpr auto kConnectAttempts = 128;
  bool accept_conns = true;
  std::atomic<int> conns_{0};
  Endpoint addr_ = testing::PickAvailableEndpoint();
  RefPtr<NativeAcceptor> acceptor_;
  RefPtr<NativeStreamConnection> server_conns_[kConnectAttempts];
};

TEST_F(NativeStreamConnectionTest, Echo) {
  static const std::string kData = "hello";
  std::atomic<int> replied{0};
  RefPtr<NativeStreamConnection> clients[kConnectAttempts];
  for (int i = 0; i != kConnectAttempts; ++i) {
    auto fd = io::util::CreateStreamSocket(addr_.Family());
    io::util::SetNonBlocking(fd.Get());
    io::util::SetCloseOnExec(fd.Get());
    io::util::StartConnect(fd.Get(), addr_);
    NativeStreamConnection::Options opts;
    opts.handler = std::make_unique<ConnectionHandler>(
        Format("client handler {}", i), [&](NoncontiguousBuffer* buffer) {
          if (buffer->ByteSize() != kData.size()) {
            return StreamConnectionHandler::DataConsumptionStatus::Ready;
          }
          [&] { ASSERT_EQ(kData, FlattenSlow(*buffer)); }();
          ++replied;
          return StreamConnectionHandler::DataConsumptionStatus::Ready;
        });
    opts.read_buffer_size = 111111;
    clients[i] =
        MakeRefCounted<NativeStreamConnection>(std::move(fd), std::move(opts));
    GetGlobalEventLoop(0, clients[i]->fd())->AttachDescriptor(clients[i].Get());
    clients[i]->StartHandshaking();
    clients[i]->Write(CreateBufferSlow(kData), 0);
  }
  while (replied != kConnectAttempts) {
    std::this_thread::sleep_for(100ms);
  }
  ASSERT_EQ(kConnectAttempts, replied.load());
  for (auto&& c : clients) {
    c->Stop();
    c->Join();
  }
}

TEST_F(NativeStreamConnectionTest, EchoWithHeavilyFragmentedBuffer) {
  NoncontiguousBuffer buffer;
  for (int i = 0; i != 60'000; ++i) {
    buffer.Append(MakeForeignBuffer(std::string(1, i % 26 + 'a')));
  }

  RefPtr<NativeStreamConnection> client;
  std::string received;
  std::atomic<int> bytes_received{};
  auto fd = io::util::CreateStreamSocket(addr_.Family());
  io::util::SetNonBlocking(fd.Get());
  io::util::SetCloseOnExec(fd.Get());
  io::util::StartConnect(fd.Get(), addr_);
  NativeStreamConnection::Options opts;
  opts.handler =
      std::make_unique<ConnectionHandler>("", [&](NoncontiguousBuffer* buffer) {
        bytes_received += buffer->ByteSize();
        received += FlattenSlow(*buffer);
        buffer->Clear();
        return StreamConnectionHandler::DataConsumptionStatus::Ready;
      });
  opts.read_buffer_size = 111111;
  client =
      MakeRefCounted<NativeStreamConnection>(std::move(fd), std::move(opts));
  GetGlobalEventLoop(0, client->fd())->AttachDescriptor(client.Get());
  client->StartHandshaking();
  client->Write(buffer, 0);
  while (bytes_received != buffer.ByteSize()) {
    std::this_thread::sleep_for(100ms);
  }
  ASSERT_EQ(FlattenSlow(buffer), received);
  client->Stop();
  client->Join();
}

TEST_F(NativeStreamConnectionTest, RemoteClose) {
  accept_conns = false;
  ClosedConnectionHandler cch;
  NativeStreamConnection::Options opts;
  opts.handler = MaybeOwning(non_owning, &cch);
  opts.read_buffer_size = 111111;
  auto fd = io::util::CreateStreamSocket(addr_.Get()->sa_family);
  io::util::SetNonBlocking(fd.Get());
  io::util::SetCloseOnExec(fd.Get());
  io::util::StartConnect(fd.Get(), addr_);
  auto fdv = fd.Get();
  auto sc =
      MakeRefCounted<NativeStreamConnection>(std::move(fd), std::move(opts));
  GetGlobalEventLoop(0, fdv)->AttachDescriptor(sc.Get());
  sc->StartHandshaking();
  std::this_thread::sleep_for(100ms);
  EXPECT_EQ(1, cch.closed.load());
  sc->Stop();
  sc->Join();
}

TEST(NativeStreamConnection, ConnectionFailure) {
  ErrorConnectionHandler ech;
  NativeStreamConnection::Options opts;
  opts.handler = MaybeOwning(non_owning, &ech);
  opts.read_buffer_size = 111111;
  auto fd = io::util::CreateStreamSocket(AF_INET);
  // Hopefully no one is listening there.
  auto invalid = EndpointFromIpv4("127.0.0.1", 1);
  io::util::SetNonBlocking(fd.Get());
  io::util::SetCloseOnExec(fd.Get());
  io::util::StartConnect(fd.Get(), invalid);
  auto fdv = fd.Get();
  auto sc =
      MakeRefCounted<NativeStreamConnection>(std::move(fd), std::move(opts));
  GetGlobalEventLoop(0, fdv)->AttachDescriptor(sc.Get());
  sc->StartHandshaking();
  std::this_thread::sleep_for(100ms);
  ASSERT_EQ(1, ech.err.load());
  sc->Stop();
  sc->Join();
}

TEST(NativeStreamConnection, NoBandwidthLimit) {
  constexpr auto kBodySize = 64 * 1024 * 1024;
  auto addr = testing::PickAvailableEndpoint();

  // Server side.
  std::atomic<std::size_t> received = 0;
  RefPtr<NativeStreamConnection> server_conn;
  auto acceptor = [&] {
    auto listen_fd = io::util::CreateListener(addr, 100);
    CHECK(listen_fd);
    NativeAcceptor::Options opts;
    opts.connection_handler = [&](Handle fd, const Endpoint& peer) {
      io::util::SetNonBlocking(fd.Get());
      io::util::SetCloseOnExec(fd.Get());
      NativeStreamConnection::Options opts;
      opts.read_buffer_size = kBodySize;
      opts.handler = std::make_unique<ConnectionHandler>(
          Format("server handler "), [&](NoncontiguousBuffer* buffer) {
            received += buffer->ByteSize();
            buffer->Clear();  // All consumed.
            return StreamConnectionHandler::DataConsumptionStatus::Ready;
          });
      server_conn = MakeRefCounted<NativeStreamConnection>(std::move(fd),
                                                           std::move(opts));
      GetGlobalEventLoop(0, server_conn->fd())
          ->AttachDescriptor(server_conn.Get());
      server_conn->StartHandshaking();
    };
    io::util::SetNonBlocking(listen_fd.Get());
    io::util::SetCloseOnExec(listen_fd.Get());
    return std::make_unique<NativeAcceptor>(std::move(listen_fd),
                                            std::move(opts));
  }();
  GetGlobalEventLoop(0, acceptor->fd())->AttachDescriptor(acceptor.get());

  // Client side.
  auto client_conn = [&] {
    auto fd = io::util::CreateStreamSocket(addr.Family());
    io::util::SetNonBlocking(fd.Get());
    io::util::SetCloseOnExec(fd.Get());
    io::util::StartConnect(fd.Get(), addr);
    NativeStreamConnection::Options opts;
    opts.handler = std::make_unique<ConnectionHandler>(
        Format("client handler"), [&](NoncontiguousBuffer* buffer) {
          CHECK(!"Nothing should be echo-d back.");
          return StreamConnectionHandler::DataConsumptionStatus::Ready;
        });
    opts.read_buffer_size = kBodySize;
    return MakeRefCounted<NativeStreamConnection>(std::move(fd),
                                                  std::move(opts));
  }();
  GetGlobalEventLoop(0, client_conn->fd())->AttachDescriptor(client_conn.Get());
  client_conn->StartHandshaking();
  auto start = ReadSteadyClock();
  client_conn->Write(CreateBufferSlow(std::string(kBodySize, 1)), 0);
  while (received.load() != kBodySize) {
    this_fiber::SleepFor(1ms);
  }
  auto time_use = ReadSteadyClock() - start;

  ASSERT_LE(time_use / 1ms, 10s / 1ms);  // 10s should be far more than enough.
  acceptor->Stop();
  acceptor->Join();
  server_conn->Stop();
  server_conn->Join();
  client_conn->Stop();
  client_conn->Join();
}

TEST(NativeStreamConnection, WriteBandwidthLimit) {
  constexpr auto kBodySize = 64 * 1024 * 1024;
  constexpr auto kBandwidthLimitMbps = 64;
  auto addr = testing::PickAvailableEndpoint();

  // Server side.
  std::atomic<std::size_t> received = 0;
  RefPtr<NativeStreamConnection> server_conn;
  auto acceptor = [&] {
    auto listen_fd = io::util::CreateListener(addr, 100);
    CHECK(listen_fd);
    NativeAcceptor::Options opts;
    opts.connection_handler = [&](Handle fd, const Endpoint& peer) {
      io::util::SetNonBlocking(fd.Get());
      io::util::SetCloseOnExec(fd.Get());
      NativeStreamConnection::Options opts;
      opts.read_buffer_size = kBodySize;
      opts.handler = std::make_unique<ConnectionHandler>(
          Format("server handler "), [&](NoncontiguousBuffer* buffer) {
            received += buffer->ByteSize();
            buffer->Clear();  // All consumed.
            return StreamConnectionHandler::DataConsumptionStatus::Ready;
          });
      server_conn = MakeRefCounted<NativeStreamConnection>(std::move(fd),
                                                           std::move(opts));
      GetGlobalEventLoop(0, server_conn->fd())
          ->AttachDescriptor(server_conn.Get());
      server_conn->StartHandshaking();
    };
    io::util::SetNonBlocking(listen_fd.Get());
    io::util::SetCloseOnExec(listen_fd.Get());
    return std::make_unique<NativeAcceptor>(std::move(listen_fd),
                                            std::move(opts));
  }();
  GetGlobalEventLoop(0, acceptor->fd())->AttachDescriptor(acceptor.get());

  // Client side.
  auto client_conn = [&] {
    auto fd = io::util::CreateStreamSocket(addr.Family());
    io::util::SetNonBlocking(fd.Get());
    io::util::SetCloseOnExec(fd.Get());
    io::util::StartConnect(fd.Get(), addr);
    NativeStreamConnection::Options opts;
    opts.handler = std::make_unique<ConnectionHandler>(
        Format("client handler"), [&](NoncontiguousBuffer* buffer) {
          CHECK(!"Nothing should be echo-d back.");
          return StreamConnectionHandler::DataConsumptionStatus::Ready;
        });
    opts.read_buffer_size = kBodySize;
    opts.write_rate_limiter = std::make_unique<TokenBucketRateLimiter>(
        kBandwidthLimitMbps * 1024 * 1024 / 8,
        kBandwidthLimitMbps * 1024 * 1024 / 8 / (1s / 1ms));
    return MakeRefCounted<NativeStreamConnection>(std::move(fd),
                                                  std::move(opts));
  }();
  GetGlobalEventLoop(0, client_conn->fd())->AttachDescriptor(client_conn.Get());
  client_conn->StartHandshaking();
  auto start = ReadSteadyClock();
  client_conn->Write(CreateBufferSlow(std::string(kBodySize, 1)), 0);
  while (received.load() != kBodySize) {
    this_fiber::SleepFor(1ms);
  }
  auto time_use = ReadSteadyClock() - start;
  auto expect_time_usage_in_seconds =
      kBodySize / (kBandwidthLimitMbps * 1024 * 1024 / 8)  // bytes / Bps
      - 1;  // Initially we allow `kBandwidthLimitMbps * 1s` to be transmitted
            // without wait.

  ASSERT_NEAR(time_use / 1ms, expect_time_usage_in_seconds * (1s / 1ms),
              1s / 1ms);
  acceptor->Stop();
  acceptor->Join();
  server_conn->Stop();
  server_conn->Join();
  client_conn->Stop();
  client_conn->Join();
}

TEST(NativeStreamConnection, ReadBandwidthLimit) {
  constexpr auto kBodySize = 64 * 1024 * 1024;
  constexpr auto kBandwidthLimitMbps = 64;
  auto addr = testing::PickAvailableEndpoint();

  // Server side.
  std::atomic<std::size_t> received = 0;
  RefPtr<NativeStreamConnection> server_conn;
  auto acceptor = [&] {
    auto listen_fd = io::util::CreateListener(addr, 100);
    CHECK(listen_fd);
    NativeAcceptor::Options opts;
    opts.connection_handler = [&](Handle fd, const Endpoint& peer) {
      io::util::SetNonBlocking(fd.Get());
      io::util::SetCloseOnExec(fd.Get());
      NativeStreamConnection::Options opts;
      opts.read_buffer_size = kBodySize;
      opts.handler = std::make_unique<ConnectionHandler>(
          Format("server handler "), [&](NoncontiguousBuffer* buffer) {
            received += buffer->ByteSize();
            buffer->Clear();  // All consumed.
            return StreamConnectionHandler::DataConsumptionStatus::Ready;
          });
      opts.read_rate_limiter = std::make_unique<TokenBucketRateLimiter>(
          kBandwidthLimitMbps * 1024 * 1024 / 8,
          kBandwidthLimitMbps * 1024 * 1024 / 8 / (1s / 1ms));
      server_conn = MakeRefCounted<NativeStreamConnection>(std::move(fd),
                                                           std::move(opts));
      GetGlobalEventLoop(0, server_conn->fd())
          ->AttachDescriptor(server_conn.Get());
      server_conn->StartHandshaking();
    };
    io::util::SetNonBlocking(listen_fd.Get());
    io::util::SetCloseOnExec(listen_fd.Get());
    return std::make_unique<NativeAcceptor>(std::move(listen_fd),
                                            std::move(opts));
  }();
  GetGlobalEventLoop(0, acceptor->fd())->AttachDescriptor(acceptor.get());

  // Client side.
  auto client_conn = [&] {
    auto fd = io::util::CreateStreamSocket(addr.Family());
    io::util::SetNonBlocking(fd.Get());
    io::util::SetCloseOnExec(fd.Get());
    io::util::StartConnect(fd.Get(), addr);
    NativeStreamConnection::Options opts;
    opts.handler = std::make_unique<ConnectionHandler>(
        Format("client handler"), [&](NoncontiguousBuffer* buffer) {
          CHECK(!"Nothing should be echo-d back.");
          return StreamConnectionHandler::DataConsumptionStatus::Ready;
        });
    opts.read_buffer_size = kBodySize;
    return MakeRefCounted<NativeStreamConnection>(std::move(fd),
                                                  std::move(opts));
  }();
  GetGlobalEventLoop(0, client_conn->fd())->AttachDescriptor(client_conn.Get());
  client_conn->StartHandshaking();
  auto start = ReadSteadyClock();
  client_conn->Write(CreateBufferSlow(std::string(kBodySize, 1)), 0);
  while (received.load() != kBodySize) {
    this_fiber::SleepFor(1ms);
  }
  auto time_use = ReadSteadyClock() - start;
  auto expect_time_usage_in_seconds =
      // bytes / Bps
      kBodySize / (kBandwidthLimitMbps * 1024 * 1024 / 8)
      // Initially we allow `kBandwidthLimitMbps * 1s` to be transmitted without
      // wait.
      - 1;

  ASSERT_NEAR(time_use / 1ms, expect_time_usage_in_seconds * (1s / 1ms),
              1s / 1ms);
  acceptor->Stop();
  acceptor->Join();
  server_conn->Stop();
  server_conn->Join();
  client_conn->Stop();
  client_conn->Join();
}

}  // namespace flare

FLARE_TEST_MAIN
