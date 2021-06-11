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

#include "flare/io/detail/writing_datagram_list.h"

#include <string>

#include "googletest/gtest/gtest.h"

#include "flare/io/util/socket.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"

using namespace std::literals;

namespace flare::io::detail {

TEST(WritingDatagramList, FlushTo) {
  static const auto ManyXs = std::string(16384, 'x');
  static const auto ManyYs = std::string(16384, 'y');

  auto port = testing::PickAvailablePort(SOCK_DGRAM);
  auto recv = util::CreateDatagramSocket(AF_INET);
  auto send = util::CreateDatagramSocket(AF_INET);
  auto addr = EndpointFromIpv4("127.0.0.1", port);
  PCHECK(bind(recv.Get(), addr.Get(), addr.Length()) == 0);

  util::SetNonBlocking(send.Get());
  WritingDatagramList wbl;
  wbl.Append(addr, CreateBufferSlow(ManyXs), 456);
  wbl.Append(addr, CreateBufferSlow(ManyYs), 567);
  bool emptied;
  std::uintptr_t ctx;
  auto rc = wbl.FlushTo(send.Get(), &ctx, &emptied);
  ASSERT_EQ(16384, rc);
  ASSERT_FALSE(emptied);
  ASSERT_EQ(456, ctx);
  rc = wbl.FlushTo(send.Get(), &ctx, &emptied);
  ASSERT_EQ(16384, rc);
  ASSERT_TRUE(emptied);
  ASSERT_EQ(567, ctx);

  char buffer[16384];
  ASSERT_EQ(16384,
            recvfrom(recv.Get(), buffer, sizeof(buffer), 0, nullptr, nullptr));
  ASSERT_EQ(0, memcmp(buffer, ManyXs.data(), 16384));
  ASSERT_EQ(16384,
            recvfrom(recv.Get(), buffer, sizeof(buffer), 0, nullptr, nullptr));
  ASSERT_EQ(0, memcmp(buffer, ManyYs.data(), 16384));
}

// It's stated that `sendmsg` may returns `EAGAIN` in certain circumstances
// (kernel buffer full?), but sending UDP via loopback always succeeded (I think
// the kernel just dropped the packets), never returns `EAGAIN`. This may need
// further investigation.
//
// @sa: https://linux.die.net/man/2/sendmsg
TEST(WritingDatagramList, DISABLED_ShortWrite) {
  auto port = testing::PickAvailablePort(SOCK_DGRAM);
  auto recv = util::CreateDatagramSocket(AF_INET);
  auto send = util::CreateDatagramSocket(AF_INET);
  auto addr = EndpointFromIpv4("127.0.0.1", port);
  PCHECK(bind(recv.Get(), addr.Get(), addr.Length()) == 0);

  util::SetNonBlocking(recv.Get());
  util::SetNonBlocking(send.Get());
  // Write datagram for 16M.
  for (int i = 0; i != 1024; ++i) {
    auto buffer = std::string(16384, 'x');
    sendto(send.Get(), buffer.data(), buffer.size(), 0, addr.Get(),
           addr.Length());
  }
  WritingDatagramList wbl;
  wbl.Append(addr, CreateBufferSlow(std::string(16384, 'x')), 456);
  bool emptied;
  std::uintptr_t ctx;
  auto rc = wbl.FlushTo(send.Get(), &ctx, &emptied);
  ASSERT_EQ(0, rc);
  ASSERT_TRUE(errno == EAGAIN || errno == EWOULDBLOCK);
  ASSERT_FALSE(emptied);
}

}  // namespace flare::io::detail

FLARE_TEST_MAIN
