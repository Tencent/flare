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

#ifndef FLARE_BASE_THREAD_THREAD_LOCAL_DETAIL_ATOMIC_PTR_H_
#define FLARE_BASE_THREAD_THREAD_LOCAL_DETAIL_ATOMIC_PTR_H_

#include "flare/base/internal/copyable_atomic.h"
#include "flare/base/ref_ptr.h"

namespace flare::tls::detail {

// Smart pointers that are safe to be read by multiple threads simultaneously.
//
// Note that it's still UNSAFE to assign to them from multiple threads. To be
// precise, it's UNSAFE to assign to this pointer in ANY FASHION at all.
// Preventing accessing it concurrently in unsafe fashion is done by the user of
// these "atomic" pointer types.
//
// They're not intended for general use. The only reason they're here is for
// `ThreadLocal` to implement `ForEach` in a thread-safe manner.

// `RefPtr`, with assignment and read being implemented by `CopyableAtomic`.
template <class T>
class AtomicRefPtr {
  using Traits = RefTraits<T>;

 public:
  AtomicRefPtr() = default;

  // Used only when relocating TLS array. Performance doesn't matter.
  AtomicRefPtr(AtomicRefPtr&& from) noexcept
      : ptr_(from.ptr_.load(std::memory_order_acquire)) {
    from.ptr_.store(nullptr, std::memory_order_relaxed);
  }

  ~AtomicRefPtr() { Clear(); }

  void Clear() {
    if (auto ptr = ptr_.load(std::memory_order_acquire)) {
      Traits::Dereference(ptr);
    }
    ptr_.store(nullptr, std::memory_order_relaxed);
  }

  void Set(RefPtr<T> from) noexcept {
    Clear();
    ptr_.store(from.Leak(), std::memory_order_release);
  }

  T* Get() const noexcept { return ptr_.load(std::memory_order_acquire); }
  T* Leak() noexcept {
    auto ptr = ptr_.load(std::memory_order_acquire);
    ptr_.store(nullptr, std::memory_order_relaxed);
    return ptr;
  }

  AtomicRefPtr& operator=(const AtomicRefPtr&) = default;

 private:
  internal::CopyableAtomic<T*> ptr_{};
};

// Non-shared ownership, helps implementing `ThreadLocal<T>`.
template <class T>
class AtomicScopedPtr {
 public:
  AtomicScopedPtr() = default;

  // Used only when relocating TLS array.
  AtomicScopedPtr(AtomicScopedPtr&& from) noexcept
      : ptr_(from.ptr_.load(std::memory_order_acquire)) {
    from.ptr_.store(nullptr, std::memory_order_relaxed);
  }

  ~AtomicScopedPtr() { Clear(); }

  void Clear() {
    if (auto ptr = ptr_.load(std::memory_order_acquire)) {
      delete ptr;
    }
    ptr_.store(nullptr, std::memory_order_relaxed);
  }

  void Set(std::unique_ptr<T> ptr) {
    Clear();
    ptr_.store(ptr.release(), std::memory_order_release);
  }

  T* Get() const noexcept { return ptr_.load(std::memory_order_acquire); }
  T* Leak() noexcept {
    auto ptr = ptr_.load(std::memory_order_acquire);
    ptr_.store(nullptr, std::memory_order_relaxed);
    return ptr;
  }

  AtomicScopedPtr& operator=(const AtomicScopedPtr&) = delete;

 private:
  internal::CopyableAtomic<T*> ptr_{};
};

}  // namespace flare::tls::detail

#endif  // FLARE_BASE_THREAD_THREAD_LOCAL_DETAIL_ATOMIC_PTR_H_
