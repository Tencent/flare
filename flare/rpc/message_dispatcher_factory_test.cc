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

template <int X>
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

class DummyLoadBalancer : public flare::LoadBalancer {
 public:
  DummyLoadBalancer() { ++instances; }
  void SetPeers(std::vector<flare::Endpoint> addresses) override {}
  bool GetPeer(std::uint64_t key, flare::Endpoint* addr,
               std::uintptr_t* ctx) override {
    return false;
  }
  void Report(const flare::Endpoint& addr, Status status,
              std::chrono::nanoseconds time_cost, std::uintptr_t ctx) override {
  }

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

[[gnu::constructor]] void InitializeFactories() {
  flare::RegisterMessageDispatcherFactoryFor(
      "boring-subsys", "scheme1", 0,
      [](auto&& uri) -> std::unique_ptr<flare::MessageDispatcher> {
        if (flare::StartsWith(uri, "first:")) {
          return std::make_unique<dummy::DummyMessageDispatcher<0>>();
        }
        return nullptr;
      });

  // Never used. It's added later than the factory above, and both handle prefix
  // `scheme1://first:`.
  flare::RegisterMessageDispatcherFactoryFor(
      "boring-subsys", "scheme1", 0,
      [](auto&& uri) -> std::unique_ptr<flare::MessageDispatcher> {
        if (flare::StartsWith(uri, "first:")) {
          return std::make_unique<dummy::DummyMessageDispatcher<1>>();
        }
        return nullptr;
      });

  flare::RegisterMessageDispatcherFactoryFor(
      "boring-subsys", "scheme1",
      0 /* Doesn't matter as it handles a different prefix. */,
      [](auto&& uri) -> std::unique_ptr<flare::MessageDispatcher> {
        if (flare::StartsWith(uri, "second:")) {
          return std::make_unique<dummy::DummyMessageDispatcher<1>>();
        }
        return nullptr;
      });

  flare::SetCatchAllMessageDispatcherFor(
      "boring-subsys",
      [](auto&& scheme,
         auto&& address) -> std::unique_ptr<flare::MessageDispatcher> {
        if (scheme == "catch-all") {
          return std::make_unique<dummy::DummyMessageDispatcher<2>>();
        }
        return nullptr;
      });
}

}  // namespace dummy

FLARE_RPC_REGISTER_LOAD_BALANCER("dummy", dummy::DummyLoadBalancer);
FLARE_RPC_REGISTER_NAME_RESOLVER("dummy", dummy::DummyNameResolver);

namespace flare {

TEST(MessageDispatcherFactory, DefaultFactory) {
  int x = 0;

  SetDefaultMessageDispatcherFactory(
      [&](auto&& subsys, auto&& scheme, auto&& addr) {
        x = 1;
        EXPECT_EQ("something", subsys);
        EXPECT_EQ("x", scheme);
        EXPECT_EQ("something-else", addr);
        return nullptr;
      });
  ASSERT_EQ(0, x);
  MakeMessageDispatcher("something", "x://something-else");
  ASSERT_EQ(1, x);
}

TEST(MessageDispatcherFactory, PreinstalledFactory) {
  SetDefaultMessageDispatcherFactory([&](auto&&...) { return nullptr; });

  EXPECT_TRUE(dynamic_cast<dummy::DummyMessageDispatcher<1>*>(
      MakeMessageDispatcher("boring-subsys", "scheme1://second:123").get()));
  EXPECT_TRUE(dynamic_cast<dummy::DummyMessageDispatcher<0>*>(
      MakeMessageDispatcher("boring-subsys", "scheme1://first:123").get()));
  EXPECT_TRUE(dynamic_cast<dummy::DummyMessageDispatcher<2>*>(
      MakeMessageDispatcher("boring-subsys", "catch-all://first:123").get()));
  EXPECT_TRUE(MakeMessageDispatcher("boring-subsys2", "x://y") == nullptr);
}

TEST(MessageDispatcherFactory, MakeComposited) {
  auto ptr = MakeCompositedMessageDispatcher("dummy", "dummy");
  EXPECT_EQ(1, dummy::DummyLoadBalancer::instances);
  EXPECT_EQ(1, dummy::DummyNameResolver::instances);
}

}  // namespace flare
