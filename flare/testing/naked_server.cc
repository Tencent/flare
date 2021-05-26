// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "flare/testing/naked_server.h"

#include <mutex>
#include <utility>

#include "flare/fiber/runtime.h"
#include "flare/io/event_loop.h"
#include "flare/io/util/socket.h"

namespace flare::testing {

class NakedServer::ConnectionHandler : public StreamConnectionHandler {
 public:
  explicit ConnectionHandler(NakedServer* server) : server_(server) {}

  void OnAttach(StreamConnection* conn) override { conn_ = conn; }
  void OnDetach() override {}

  void OnWriteBufferEmpty() override {}
  void OnDataWritten(std::uintptr_t ctx) override {}

  DataConsumptionStatus OnDataArrival(NoncontiguousBuffer* buffer) override {
    return server_->handler_(conn_, buffer) ? DataConsumptionStatus::Ready
                                            : DataConsumptionStatus::Error;
  }

  void OnClose() override {}
  void OnError() override {}

 private:
  NakedServer* server_;
  StreamConnection* conn_;
};

NakedServer::~NakedServer() {
  if (!stopped_) {
    Stop();
    Join();
  }
}

void NakedServer::SetHandler(
    Function<bool(StreamConnection*, NoncontiguousBuffer*)> handler) {
  handler_ = std::move(handler);
}

void NakedServer::ListenOn(const Endpoint& addr) { listening_on_ = addr; }

void NakedServer::Start() {
  NativeAcceptor::Options options = {
      .connection_handler = [this](auto&&... args) {
        OnConnection(std::move(args)...);
      }};

  acceptor_ = MakeRefCounted<NativeAcceptor>(
      io::util::CreateListener(listening_on_, 128), std::move(options));
  io::util::SetNonBlocking(acceptor_->fd());
  io::util::SetCloseOnExec(acceptor_->fd());
  GetGlobalEventLoop(fiber::GetCurrentSchedulingGroupIndex(), acceptor_->fd())
      ->AttachDescriptor(acceptor_.Get());
}

void NakedServer::Stop() {
  stopped_ = true;
  acceptor_->Stop();
  std::scoped_lock _(conns_lock_);
  for (auto&& e : conns_) {
    e->Stop();
  }
}

void NakedServer::Join() {
  acceptor_->Join();
  for (auto&& e : conns_) {
    e->Join();
  }
}

void NakedServer::OnConnection(Handle fd, Endpoint peer) {
  static std::atomic<std::size_t> next_scheduling_group{};

  NativeStreamConnection::Options options = {
      .handler = std::make_unique<ConnectionHandler>(this),
      .read_buffer_size = 1677216};
  auto conn =
      MakeRefCounted<NativeStreamConnection>(std::move(fd), std::move(options));
  io::util::SetNonBlocking(conn->fd());
  io::util::SetCloseOnExec(conn->fd());
  io::util::SetTcpNoDelay(conn->fd());

  std::scoped_lock _(conns_lock_);
  conns_.push_back(conn);
  GetGlobalEventLoop(next_scheduling_group++ % fiber::GetSchedulingGroupCount(),
                     conn->fd())
      ->AttachDescriptor(conn.Get());
  conn->StartHandshaking();
}

}  // namespace flare::testing
