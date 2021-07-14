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

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "flare/base/internal/hash_map.h"
#include "flare/base/never_destroyed.h"
#include "flare/rpc/load_balancer/load_balancer.h"
#include "flare/rpc/message_dispatcher/composited.h"
#include "flare/rpc/name_resolver/name_resolver.h"

using namespace std::literals;

namespace flare {

namespace {

// [Priority, Factory].
using Factories = std::vector<std::pair<
    int, Function<std::unique_ptr<MessageDispatcher>(std::string_view)>>>;

using MessageDispatcherFactory =
    Function<std::unique_ptr<MessageDispatcher>(std::string_view)>;
using CatchAllMessageDispatcherFactory =
    Function<std::unique_ptr<MessageDispatcher>(std::string_view,
                                                std::string_view)>;
using DefaultMessageDispatcherFactory =
    Function<std::unique_ptr<MessageDispatcher>(
        std::string_view, std::string_view, std::string_view)>;

// I don't expect too many different factories for a given scheme. In this case
// linear-scan on a vector should perform better than (unordered_)map lookup.
internal::HashMap<std::string, std::vector<std::pair<std::string, Factories>>>*
GetFactoryRegistry() {
  static NeverDestroyed<internal::HashMap<
      std::string, std::vector<std::pair<std::string, Factories>>>>
      registry;
  return registry.Get();
}

// Usable only at startup. If the given subsystem or scheme is not recognized,
// it's added.
Factories* GetOrCreateFactoriesForWriteUnsafe(std::string_view subsys,
                                              std::string_view scheme) {
  auto&& per_subsys = (*GetFactoryRegistry())[subsys];

  for (auto&& [k, factories] : per_subsys) {
    if (k == scheme) {
      return &factories;
    }
  }
  auto&& added = per_subsys.emplace_back();
  added.first = scheme;
  return &added.second;
}

// If the given subsystem or scheme is not recognized, `nullptr` is returned.
const Factories* GetFactoriesForRead(std::string_view subsys,
                                     std::string_view scheme) {
  auto ptr = GetFactoryRegistry()->TryGet(subsys);
  if (!ptr) {
    return nullptr;
  }
  for (auto&& [k, factories] : *ptr) {
    if (k == scheme) {
      return &factories;
    }
  }
  return nullptr;
}

CatchAllMessageDispatcherFactory* GetCatchAllMessageDispatcherFactoryFor(
    std::string_view subsys) {
  static NeverDestroyed<
      internal::HashMap<std::string, CatchAllMessageDispatcherFactory>>
      registry;
  return &(*registry)[subsys];
}

DefaultMessageDispatcherFactory* GetDefaultMessageDispatcherFactory() {
  static DefaultMessageDispatcherFactory factory =
      [](auto&& subsys, auto&& scheme,
         auto&& address) -> std::unique_ptr<MessageDispatcher> {
    FLARE_LOG_ERROR_EVERY_SECOND(
        "No message dispatcher factory is provided for subsystem [{}], uri "
        "[{}://{}].",
        subsys, scheme, address);
    return nullptr;
  };
  return &factory;
}

}  // namespace

std::unique_ptr<MessageDispatcher> MakeMessageDispatcher(
    std::string_view subsys, std::string_view uri) {
  static constexpr auto kSep = "://"sv;

  auto pos = uri.find_first_of(kSep);
  FLARE_CHECK_NE(pos, std::string_view::npos, "No `scheme` found in URI [{}].",
                 uri);
  auto scheme = uri.substr(0, pos);
  auto address = uri.substr(pos + kSep.size());
  if (auto factories = GetFactoriesForRead(subsys, scheme)) {
    for (auto&& [_ /* priority */, e] : *factories) {
      if (auto ptr = e(address)) {
        return ptr;
      }
    }
  }
  if (auto&& catch_all = *GetCatchAllMessageDispatcherFactoryFor(subsys)) {
    if (auto ptr = catch_all(scheme, address)) {
      return ptr;
    }
  }
  return (*GetDefaultMessageDispatcherFactory())(subsys, scheme, address);
}

void RegisterMessageDispatcherFactoryFor(
    const std::string& subsys, const std::string& scheme, int priority,
    Function<std::unique_ptr<MessageDispatcher>(std::string_view)> factory) {
  auto&& factories = GetOrCreateFactoriesForWriteUnsafe(subsys, scheme);
  factories->emplace_back(priority, std::move(factory));

  // If multiple factories with the same priority present, we'd like to make
  // sure the one added first is of higher priority.
  std::stable_sort(factories->begin(), factories->end(),
                   [](auto&& x, auto&& y) { return x.first < y.first; });
}

void SetCatchAllMessageDispatcherFor(
    const std::string& subsys,
    Function<std::unique_ptr<MessageDispatcher>(std::string_view,
                                                std::string_view)>
        factory) {
  *GetCatchAllMessageDispatcherFactoryFor(subsys) = std::move(factory);
  // I'm not sure if we should return the old factory. Is it of any use?
}

void SetDefaultMessageDispatcherFactory(
    Function<std::unique_ptr<MessageDispatcher>(
        std::string_view, std::string_view, std::string_view)>
        factory) {
  *GetDefaultMessageDispatcherFactory() = std::move(factory);
}

std::unique_ptr<MessageDispatcher> MakeCompositedMessageDispatcher(
    std::string_view resolver, std::string_view load_balancer) {
  auto r = name_resolver_registry.TryGet(resolver);
  auto lb = load_balancer_registry.TryNew(load_balancer);
  if (!r || !lb) {
    return nullptr;
  }
  return std::make_unique<message_dispatcher::Composited>(r, std::move(lb));
}

}  // namespace flare
