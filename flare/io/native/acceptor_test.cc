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

#include "flare/io/native/acceptor.h"

#include <thread>
#include <utility>

#include "gtest/gtest.h"

#include "flare/io/event_loop.h"
#include "flare/io/util/socket.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"

using namespace std::literals;

namespace flare {

TEST(NativeAcceptor, All) {
  // Default of `net.core.somaxconn`.
  //
  // @sa: https://serverfault.com/q/518862
  constexpr auto kConnectAttempts = 128;

  std::atomic<int> conns = 0;
  auto addr = testing::PickAvailableEndpoint();
  // backlog must be large enough for connections below (made in burst) to
  // succeeded.
  auto listen_fd = io::util::CreateListener(addr, kConnectAttempts);
  CHECK(listen_fd);
  NativeAcceptor::Options opts;
  opts.connection_handler = [&](Handle fd, const Endpoint& peer) { ++conns; };
  io::util::SetNonBlocking(listen_fd.Get());
  io::util::SetCloseOnExec(listen_fd.Get());
  auto fdv = listen_fd.Get();
  auto acceptor =
      MakeRefCounted<NativeAcceptor>(std::move(listen_fd), std::move(opts));
  GetGlobalEventLoop(0, fdv)->AttachDescriptor(acceptor.Get());

  Handle clients[kConnectAttempts];
  for (int i = 0; i != kConnectAttempts; ++i) {
    clients[i] = io::util::CreateStreamSocket(addr.Get()->sa_family);
    io::util::SetNonBlocking(clients[i].Get());
    io::util::SetCloseOnExec(clients[i].Get());
    io::util::StartConnect(clients[i].Get(), addr);
  }
  std::cout << "All connection attempts made." << std::endl;
  while (conns != kConnectAttempts) {
    std::this_thread::sleep_for(1ms);
  }
  ASSERT_EQ(kConnectAttempts, conns.load());
  acceptor->Stop();
  acceptor->Join();
}

}  // namespace flare

FLARE_TEST_MAIN
