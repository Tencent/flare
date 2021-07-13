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

#include "flare/rpc/message_dispatcher_factory.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "flare/base/string.h"
#include "flare/rpc/load_balancer/load_balancer.h"
#include "flare/rpc/name_resolver/name_resolver.h"

namespace dummy {

class DummyMessageDispatcher : public flare::MessageDispatcher {
 public:
  bool Open(const std::string& name) override { return true; }
  bool GetPeer(std::uint64_t key, flare::Endpoint* addr,
               std::uintptr_t* ctx) override {
    return false;
  }
  void Report(const flare::Endpoint& addr, Status status,
              std::chrono::nanoseconds time_cost, std::uintptr_t ctx) override {
  }
};

class DummyMessageDispatcher2 : public flare::MessageDispatcher {
 public:
  bool Open(const std::string& name) override { return true; }
  bool GetPeer(std::uint64_t key, flare::Endpoint* addr,
               std::uintptr_t* ctx) override {
    return false;
  }
  void Report(const flare::Endpoint& addr, Status status,
              std::chrono::nanoseconds time_cost, std::uintptr_t ctx) override {
  }
};

class DummyLoadBalancer : public flare::LoadBalancer {
 public:
  DummyLoadBalancer() { ++instances; }
  void SetPeers(std::vector<flare::Endpoint> addresses) override {}
  bool GetPeer(std::uint64_t key, flare::Endpoint* addr,
               std::uintptr_t* ctx) override {
    return false;
  }
  void Report(const flare::Endpoint& addr, Status status,
              std::chrono::nanoseconds time_cost, std::uintptr_t ctx) {}

  inline static int instances;
};

class DummyNameResolver : public flare::NameResolver {
 public:
  DummyNameResolver() { ++instances; }
  std::unique_ptr<flare::NameResolutionView> StartResolving(
      const std::string& name) override {
    return nullptr;
  }

  inline static int instances;
};

}  // namespace dummy

FLARE_RPC_REGISTER_LOAD_BALANCER("dummy", dummy::DummyLoadBalancer);
FLARE_RPC_REGISTER_NAME_RESOLVER("dummy", dummy::DummyNameResolver);

namespace flare {

TEST(MessageDispatcherFactory, DefaultFactory) {
  int x = 0;

  SetDefaultMessageDispatcherFactory([&](auto&&...) {
    x = 1;
    return nullptr;
  });
  ASSERT_EQ(0, x);
  MakeMessageDispatcher("something", "x://something else");
  ASSERT_EQ(1, x);
}

TEST(MessageDispatcherFactory, PreinstalledFactory) {
  SetDefaultMessageDispatcherFactory([&](auto&&...) { return nullptr; });

  EXPECT_TRUE(dynamic_cast<dummy::DummyMessageDispatcher2*>(
      MakeMessageDispatcher("boring-subsys", "scheme1://second:123").get()));
  EXPECT_TRUE(dynamic_cast<dummy::DummyMessageDispatcher*>(
      MakeMessageDispatcher("boring-subsys", "scheme1://first:123").get()));
  EXPECT_TRUE(MakeMessageDispatcher("boring-subsys", "scheme2://first:123") ==
              nullptr);
}

TEST(MessageDispatcherFactory, MakeComposited) {
  auto ptr = MakeCompositedMessageDispatcher("dummy", "dummy");
  EXPECT_EQ(1, dummy::DummyLoadBalancer::instances);
  EXPECT_EQ(1, dummy::DummyNameResolver::instances);
}

}  // namespace flare

FLARE_RPC_REGISTER_MESSAGE_DISPATCHER_FACTORY_FOR(
    "boring-subsys", "scheme1", 0,
    [](auto&& uri) -> std::unique_ptr<flare::MessageDispatcher> {
      if (flare::StartsWith(uri, "scheme1://first:")) {
        return std::make_unique<dummy::DummyMessageDispatcher>();
      }
      return nullptr;
    });

// Never used. It's added later than the factory above, and both handle prefix
// `scheme1://first:`.
FLARE_RPC_REGISTER_MESSAGE_DISPATCHER_FACTORY_FOR(
    "boring-subsys", "scheme1", 0,
    [](auto&& uri) -> std::unique_ptr<flare::MessageDispatcher> {
      if (flare::StartsWith(uri, "scheme1://first:")) {
        return std::make_unique<dummy::DummyMessageDispatcher2>();
      }
      return nullptr;
    });

FLARE_RPC_REGISTER_MESSAGE_DISPATCHER_FACTORY_FOR(
    "boring-subsys", "scheme1",
    0 /* Doesn't matter as it handles a different prefix. */,
    [](auto&& uri) -> std::unique_ptr<flare::MessageDispatcher> {
      if (flare::StartsWith(uri, "scheme1://second:")) {
        return std::make_unique<dummy::DummyMessageDispatcher2>();
      }
      return nullptr;
    });
