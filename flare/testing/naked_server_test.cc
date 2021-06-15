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

#include <algorithm>

#include "gtest/gtest.h"

#include "flare/io/util/socket.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"

namespace flare::testing {

TEST(NakedServer, All) {
  auto server_ep = PickAvailableEndpoint();

  NakedServer server;
  server.SetHandler([](StreamConnection* conn, NoncontiguousBuffer* buffer) {
    auto flattened = FlattenSlow(*buffer);
    buffer->Clear();
    std::transform(flattened.begin(), flattened.end(), flattened.begin(),
                   [](auto&& e) { return e + 1; });
    FLARE_CHECK(conn->Write(CreateBufferSlow(flattened), 0));
    return true;
  });
  server.ListenOn(server_ep);
  server.Start();

  auto handle = io::util::CreateStreamSocket(server_ep.Family());
  FLARE_CHECK(io::util::StartConnect(handle.Get(), server_ep));
  FLARE_PCHECK(write(handle.Get(), "12345678", 8) == 8);
  char buffer[8];
  FLARE_PCHECK(read(handle.Get(), buffer, 8) == 8);
  EXPECT_EQ(0, memcmp(buffer, "23456789", 8));
}

}  // namespace flare::testing

FLARE_TEST_MAIN
