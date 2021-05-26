// Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "flare/init/on_init.h"

#include <algorithm>
#include <atomic>
#include <map>
#include <mutex>
#include <utility>
#include <vector>

#include "flare/base/function.h"
#include "flare/base/logging.h"
#include "flare/base/never_destroyed.h"
#include "flare/base/random.h"

namespace flare {

namespace {

// The below two registries are filled by `PrepareForRunningCallbacks()` (by
// moving callbacks from the registry above.)
std::vector<Function<void()>> initializer_registry;
std::vector<Function<void()>> finalizer_registry;

struct AtExitCallbackRegistry {
  std::mutex lock;
  std::vector<Function<void()>> registry;
};

AtExitCallbackRegistry* GetAtExitCallbackRegistry() {
  static NeverDestroyed<AtExitCallbackRegistry> registry;
  return registry.Get();
}

}  // namespace

}  // namespace flare

namespace flare::internal {

void SetAtExitCallback(Function<void()> callback) {
  auto&& registry = GetAtExitCallbackRegistry();
  std::scoped_lock _(registry->lock);
  registry->registry.push_back(std::move(callback));
}

}  // namespace flare::internal

namespace flare::detail {

std::atomic<bool> registry_prepared{false};

// This registry is filled by `RegisterCallback()`.
auto GetStagingRegistry() {
  // Must be `std::map`, order (of key) matters.
  static std::map<std::int32_t,
                  std::vector<std::pair<Function<void()>, Function<void()>>>>
      registry;
  return &registry;
}

void PrepareForRunningCallbacks() {
  auto&& temp_registry = *GetStagingRegistry();
  auto&& init_registry = initializer_registry;
  auto&& fini_registry = finalizer_registry;

  // Loop from lowest priority to highest.
  for (auto&& [k, v] : temp_registry) {
    // Force a shuffle here, so that the user cannot rely on relative call order
    // between same priority callbacks.
    std::shuffle(v.begin(), v.end(), random::Engine());
    for (auto&& f : v) {
      init_registry.push_back(std::move(f.first));
      if (f.second) {
        fini_registry.push_back(std::move(f.second));
      }
    }
  }

  // Finalizers are called in opposite order.
  std::reverse(fini_registry.begin(), fini_registry.end());
  registry_prepared.store(true, std::memory_order_relaxed);
}

void RunAllInitializers() {
  PrepareForRunningCallbacks();

  auto&& registry = initializer_registry;
  for (auto&& c : registry) {
    c();
  }
  // In case the initializer holds some resource (unlikely), free it now.
  registry.clear();
}

void RunAllFinalizers() {
  auto&& registry = finalizer_registry;
  for (auto&& c : registry) {
    c();
  }
  registry.clear();
  for (auto&& e : GetAtExitCallbackRegistry()->registry) {
    e();
  }
}

void RegisterOnInitCallback(std::int32_t priority, Function<void()> init,
                            Function<void()> fini) {
  FLARE_CHECK(
      !registry_prepared.load(std::memory_order_relaxed),
      "Callbacks may only be registered before `flare::Start` is called.");

  auto&& registry = *GetStagingRegistry();
  registry[priority].emplace_back(std::move(init), std::move(fini));
}

}  // namespace flare::detail
