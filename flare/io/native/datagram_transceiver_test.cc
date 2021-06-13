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

#include "flare/io/native/datagram_transceiver.h"

#include <sys/signal.h>

#include <string>
#include <utility>

#include "gtest/gtest.h"

#include "flare/io/event_loop.h"
#include "flare/io/util/socket.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"

using namespace std::literals;

namespace flare {

struct Handler : DatagramTransceiverHandler {
  void OnAttach(DatagramTransceiver*) override {}
  void OnDetach() override {}
  void OnPendingWritesFlushed() override {}
  void OnDatagramWritten(std::uintptr_t ctx) override {}
  DataConsumptionStatus OnDatagramArrival(NoncontiguousBuffer buffer,
                                          const Endpoint& addr) override {
    data = FlattenSlow(buffer);
    recv = true;
    return DataConsumptionStatus::Consumed;
  }
  void OnError() override { PCHECK(0); }

  std::atomic<bool> recv{false};
  std::string data;
};

TEST(NativeDatagramTransceiver, SendRecv) {
  NoncontiguousBuffer buffers[2];

  buffers[0] = CreateBufferSlow("123");
  // buffers[1] is a highly fragmented one (more than `IOV_MAX` blocks).
  for (int i = 0; i != 60'000; ++i) {
    buffers[1].Append(MakeForeignBuffer(std::string(1, i % 26 + 'a')));
  }

  for (auto&& buffer : buffers) {
    auto recvside = io::util::CreateDatagramSocket(AF_INET);
    io::util::SetNonBlocking(recvside.Get());
    io::util::SetCloseOnExec(recvside.Get());
    auto sender = io::util::CreateDatagramSocket(AF_INET);
    io::util::SetNonBlocking(sender.Get());
    io::util::SetCloseOnExec(sender.Get());

    auto addr = testing::PickAvailableEndpoint(SOCK_DGRAM);
    PCHECK(bind(recvside.Get(), addr.Get(), addr.Length()) == 0);

    // Server
    NativeDatagramTransceiver::Options opts;
    Handler h;
    opts.handler = MaybeOwning(non_owning, &h);
    auto server = MakeRefCounted<NativeDatagramTransceiver>(std::move(recvside),
                                                            std::move(opts));
    GetGlobalEventLoop(0, server->fd())->AttachDescriptor(server.Get());

    // Client
    NativeDatagramTransceiver::Options opts2;
    Handler h2;
    opts2.handler = MaybeOwning(non_owning, &h);
    auto client = MakeRefCounted<NativeDatagramTransceiver>(std::move(sender),
                                                            std::move(opts2));
    GetGlobalEventLoop(0, client->fd())->AttachDescriptor(client.Get());

    // Send
    client->Write(addr, buffer, 1234);
    while (!h.recv) {
    }
    ASSERT_EQ(FlattenSlow(buffer), h.data);

    // Destruction
    server->Stop();
    client->Stop();
    server->Join();
    client->Join();
  }
}

}  // namespace flare

FLARE_TEST_MAIN
