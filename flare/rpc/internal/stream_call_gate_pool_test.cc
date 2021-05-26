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

#include "flare/rpc/internal/stream_call_gate_pool.h"

#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/fiber/this_fiber.h"
#include "flare/rpc/internal/stream_call_gate.h"
#include "flare/rpc/server.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"

using namespace std::literals;

DECLARE_int32(flare_rpc_client_max_connections_per_server);
DECLARE_int32(flare_rpc_client_remove_idle_connection_interval);
DECLARE_int32(flare_rpc_client_connection_max_idle);

namespace flare::rpc::internal {

class CounterProtocol : public StreamProtocol {
 public:
  CounterProtocol() { ++alive_instances; }
  ~CounterProtocol() { --alive_instances; }

  const Characteristics& GetCharacteristics() const override {
    static Characteristics c;
    return c;
  }

  const MessageFactory* GetMessageFactory() const override {
    return MessageFactory::null_factory;
  }
  const ControllerFactory* GetControllerFactory() const override {
    return ControllerFactory::null_factory;
  }
  MessageCutStatus TryCutMessage(NoncontiguousBuffer* buffer,
                                 std::unique_ptr<Message>* message) override {
    return MessageCutStatus::Error;
  }
  bool TryParse(std::unique_ptr<Message>* message,
                Controller* controller) override {
    return false;
  }
  void WriteMessage(const Message& message, NoncontiguousBuffer* buffer,
                    Controller* controller) override {}

  static inline std::atomic<std::size_t> alive_instances = 0;
};

class StreamCallGatePoolTest : public ::testing::Test {
 public:
  void SetUp() override {
    listening_ep_ = testing::PickAvailableEndpoint();
    server_.AddProtocol("flare");
    server_.ListenOn(listening_ep_);
    server_.Start();
  }

  void TearDown() override {
    server_.Stop();
    server_.Join();
  }

 protected:
  RefPtr<StreamCallGate> CreateGate(const Endpoint& to) {
    auto gate = MakeRefCounted<StreamCallGate>();
    StreamCallGate::Options opts;
    opts.protocol = std::make_unique<CounterProtocol>();
    opts.maximum_packet_size = 1;
    gate->Open(to, std::move(opts));
    return gate;
  }

  Endpoint listening_ep_;
  Server server_;
};

static const auto kEndpoint2 = EndpointFromIpv4("192.0.2.1", 2345);

TEST_F(StreamCallGatePoolTest, CreateShared) {
  auto gate = GetGlobalStreamCallGatePool("")->GetOrCreateShared(
      listening_ep_, false, [&] { return CreateGate(listening_ep_); });
  ASSERT_EQ(1, CounterProtocol::alive_instances);
  auto p1 = gate.Get();
  gate.Close();
  auto gate2 = GetGlobalStreamCallGatePool("")->GetOrCreateShared(
      listening_ep_, false, [&] {
        CHECK(!"Never here.");
        return CreateGate(listening_ep_);
      });
  ASSERT_EQ(1, CounterProtocol::alive_instances);
  ASSERT_EQ(p1, gate2.Get());  // The gate is shared.
}

TEST_F(StreamCallGatePoolTest, CreateExclusive) {
  auto gate = GetGlobalStreamCallGatePool("")->GetOrCreateExclusive(
      listening_ep_, [&] { return CreateGate(listening_ep_); });
  auto gate2 = GetGlobalStreamCallGatePool("")->GetOrCreateExclusive(
      listening_ep_, [&] { return CreateGate(listening_ep_); });
  ASSERT_NE(gate.Get(), gate2.Get());  // The gate is NOT shared.
}

TEST_F(StreamCallGatePoolTest, RemoveIdleConnection) {
  this_fiber::SleepFor(5s);  // Let any already-created gate to expire.
  ASSERT_EQ(0, CounterProtocol::alive_instances);
  {
    auto gate = GetGlobalStreamCallGatePool("")->GetOrCreateShared(
        listening_ep_, false, [&] { return CreateGate(listening_ep_); });
    ASSERT_EQ(1, CounterProtocol::alive_instances);
  }
  this_fiber::SleepFor(3s);
  ASSERT_EQ(0, CounterProtocol::alive_instances);
}

TEST_F(StreamCallGatePoolTest, RemoveIdleConnection2) {
  this_fiber::SleepFor(5s);  // Let any already-created gate to expire.
  ASSERT_EQ(0, CounterProtocol::alive_instances);
  for (int i = 0; i != 200; ++i) {  // 2s
    auto gate = GetGlobalStreamCallGatePool("")->GetOrCreateShared(
        listening_ep_, true, [&] { return CreateGate(listening_ep_); });
    this_fiber::SleepFor(10ms);
    // The expiration time keeps renewing, so it won't be removed.
    ASSERT_EQ(1, CounterProtocol::alive_instances);
  }
  this_fiber::SleepFor(2s);
  ASSERT_EQ(0, CounterProtocol::alive_instances);
}

TEST_F(StreamCallGatePoolTest, RemoveBrokenGate) {
  auto gate = GetGlobalStreamCallGatePool("")->GetOrCreateShared(
      kEndpoint2, false, [&] { return CreateGate(kEndpoint2); });
  ASSERT_EQ(1, CounterProtocol::alive_instances);
  gate->healthy_ = false;
  gate.Close();
  ASSERT_EQ(0, CounterProtocol::alive_instances);  // Removed immediately.
}

TEST_F(StreamCallGatePoolTest, CreateExclusiveToUnreachable) {
  // Not sure if this UT still work when IPv6 is reachable.
  //
  // I've tested that 2001:db8::/32 would fail this UT if IPv6 stack is enabled.
  //
  // fe80::1 does make the connection fail (i.e., make the UT work) on (my)
  // machine, with IPv6 stack enabled (but no connectivity to Internet.).
  auto ep = EndpointFromIpv6("fe80::1", 1);
  auto gate = GetGlobalStreamCallGatePool("")->GetOrCreateExclusive(
      ep, [&] { return CreateGate(ep); });
  ASSERT_FALSE(gate->Healthy());
  gate.Close();  // This shouldn't crash.
}

}  // namespace flare::rpc::internal

int main(int argc, char** argv) {
  FLAGS_flare_rpc_client_max_connections_per_server = 1;
  FLAGS_flare_rpc_client_remove_idle_connection_interval = 1;
  FLAGS_flare_rpc_client_connection_max_idle = 1;
  return flare::testing::InitAndRunAllTests(&argc, argv);
}
