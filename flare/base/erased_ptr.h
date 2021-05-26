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

#ifndef FLARE_BASE_ERASED_PTR_H_
#define FLARE_BASE_ERASED_PTR_H_

#include <utility>

#include "flare/base/likely.h"

namespace flare {

// RAII wrapper for holding type erased pointers. Type-safety is your own
// responsibility.
class ErasedPtr final {
 public:
  using Deleter = void (*)(void*);

  // A default constructed one is an empty one.
  /* implicit */ constexpr ErasedPtr(std::nullptr_t = nullptr)
      : ptr_(nullptr), deleter_(nullptr) {}

  // Ownership taken.
  template <class T>
  constexpr explicit ErasedPtr(T* ptr) noexcept
      : ptr_(ptr), deleter_([](void* ptr) { delete static_cast<T*>(ptr); }) {}
  template <class T, class D>
  constexpr ErasedPtr(T* ptr, D deleter) noexcept
      : ptr_(ptr), deleter_(deleter) {}

  // Movable
  constexpr ErasedPtr(ErasedPtr&& ptr) noexcept
      : ptr_(ptr.ptr_), deleter_(ptr.deleter_) {
    ptr.ptr_ = nullptr;
  }

  ErasedPtr& operator=(ErasedPtr&& ptr) noexcept {
    if (FLARE_LIKELY(&ptr != this)) {
      Reset();
    }
    std::swap(ptr_, ptr.ptr_);
    std::swap(deleter_, ptr.deleter_);
    return *this;
  }

  // But not copyable.
  ErasedPtr(const ErasedPtr&) = delete;
  ErasedPtr& operator=(const ErasedPtr&) = delete;

  // Any resource we're holding is freed in dtor.
  ~ErasedPtr() {
    if (ptr_) {
      deleter_(ptr_);
    }
  }

  // Accessor.
  constexpr void* Get() const noexcept { return ptr_; }

  // It's your responsibility to check if the type match.
  template <class T>
  T* UncheckedGet() const noexcept {
    return reinterpret_cast<T*>(Get());
  }

  // Test if this object holds a valid pointer.
  constexpr explicit operator bool() const noexcept { return !!ptr_; }

  // Free any resource this class holds and reset its internal pointer to
  // `nullptr`.
  constexpr void Reset(std::nullptr_t = nullptr) noexcept {
    if (ptr_) {
      deleter_(ptr_);
      ptr_ = nullptr;
    }
  }

  // Release ownership of its internal object.
  [[nodiscard]] void* Leak() noexcept { return std::exchange(ptr_, nullptr); }

  // This is the only way you can destroy the pointer you obtain from `Leak()`.
  constexpr Deleter GetDeleter() const noexcept { return deleter_; }

 private:
  void* ptr_;
  Deleter deleter_;
};

template <class T, class... Args>
ErasedPtr MakeErased(Args&&... args) {
  return ErasedPtr(new T(std::forward<Args>(args)...));
}

}  // namespace flare

#endif  // FLARE_BASE_ERASED_PTR_H_
