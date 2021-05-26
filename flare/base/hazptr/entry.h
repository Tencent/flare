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

#ifndef FLARE_BASE_HAZPTR_ENTRY_H_
#define FLARE_BASE_HAZPTR_ENTRY_H_

#include <atomic>

namespace flare {

class HazptrDomain;

}  // namespace flare

namespace flare::hazptr {

class Object;

struct Entry {
  std::atomic<const Object*> ptr;
  std::atomic<bool> active{false};
  HazptrDomain* domain;
  Entry* next{nullptr};

  bool TryAcquire() noexcept {
    return !active.load(std::memory_order_relaxed) &&
           !active.exchange(true, std::memory_order_relaxed);
  }

  void Release() noexcept { active.store(false, std::memory_order_relaxed); }

  const Object* TryGetPtr() noexcept {
    return ptr.load(std::memory_order_acquire);
  }

  void ExposePtr(const Object* ptr) noexcept {
    this->ptr.store(ptr, std::memory_order_release);
  }
};

}  // namespace flare::hazptr

#endif  // FLARE_BASE_HAZPTR_ENTRY_H_
