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

#include "flare/testing/hooking_mock.h"

#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>

#include "flare/base/never_destroyed.h"

namespace flare::testing::detail {

MockerRegistry* MockerRegistry::Instance() {
  static NeverDestroyed<MockerRegistry> instance;
  return instance.Get();
}

void PrintCrashyImplementationErrorOnce() {
  FLARE_LOG_ERROR_ONCE(
      "Member function pointer is not the same size of generic pointer. Our "
      "implementation is likely crashy on such platform.");
}

std::shared_ptr<void> ApplyHookOn(void* from, void* to) {
  static std::mutex lock;
  static std::unordered_map<void*, std::pair<void*, std::weak_ptr<void>>>
      installed_hook;
  std::scoped_lock _(lock);
  if (auto iter = installed_hook.find(from); iter != installed_hook.end()) {
    if (auto ptr = iter->second.second.lock()) {
      FLARE_CHECK(
          iter->second.first == to,
          "Installing two hook with the same source but different target?");
      return ptr;
    }
  }
  std::shared_ptr<void> handle(
      InstallHook(reinterpret_cast<void*>(from), reinterpret_cast<void*>(to)),
      [](void* ptr) { UninstallHook(ptr); });
  installed_hook[from] = {to, handle};
  return handle;
}

}  // namespace flare::testing::detail
