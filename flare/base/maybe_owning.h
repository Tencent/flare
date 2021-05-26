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

#ifndef FLARE_BASE_MAYBE_OWNING_H_
#define FLARE_BASE_MAYBE_OWNING_H_

#include <memory>
#include <utility>

#include "thirdparty/googletest/gtest/gtest_prod.h"

#include "flare/base/internal/logging.h"

namespace flare {

inline constexpr struct owning_t { explicit owning_t() = default; } owning;
inline constexpr struct non_owning_t {
  explicit non_owning_t() = default;
} non_owning;

// You don't know if you own `T`, I don't know either.
//
// Seriously, this class allows you to have a unified way for handling pointers
// that you own and pointers that you don't own, so you don't have to defining
// pairs of methods such as `AddXxx()` and `AddAllocatedXxx()`.
template <class T>
class MaybeOwning {
 public:
  // Test if `MaybeOwning<T>` should accept `U*` in conversion.
  template <class U>
  inline static constexpr bool is_convertible_from_v =
      std::is_convertible_v<U*, T*> &&
      (std::is_same_v<std::remove_cv_t<T>, std::remove_cv_t<U>> ||
       std::has_virtual_destructor_v<T>);

  // Construct an empty one.
  constexpr MaybeOwning() noexcept : owning_(false), ptr_(nullptr) {}
  /* implicit */ MaybeOwning(std::nullptr_t) noexcept : MaybeOwning() {}

  // Maybe transferring ownership.
  //
  // The first two overloads are preferred since they're more evident about
  // what's going on.
  constexpr MaybeOwning(owning_t, T* ptr) noexcept : MaybeOwning(ptr, true) {}
  constexpr MaybeOwning(non_owning_t, T* ptr) noexcept
      : MaybeOwning(ptr, false) {}
  constexpr MaybeOwning(T* ptr, bool owning) noexcept
      : owning_(owning), ptr_(ptr) {}

  // Transferring ownership.
  //
  // This overload only participate in overload resolution if `U*` is implicitly
  // convertible to `T*`.
  template <class U, class = std::enable_if_t<is_convertible_from_v<U>>>
  /* `explicit`? */ constexpr MaybeOwning(std::unique_ptr<U> ptr) noexcept
      : owning_(!!ptr), ptr_(ptr.release()) {}

  // Transferring ownership.
  //
  // This overload only participate in overload resolution if `U*` is implicitly
  // convertible to `T*`.
  template <class U, class = std::enable_if_t<is_convertible_from_v<U>>>
  /* implicit */ constexpr MaybeOwning(MaybeOwning<U>&& ptr) noexcept
      : owning_(ptr.owning_), ptr_(ptr.ptr_) {
    ptr.owning_ = false;
    ptr.ptr_ = nullptr;
  }

  // Movable.
  constexpr MaybeOwning(MaybeOwning&& other) noexcept
      : owning_(other.owning_), ptr_(other.ptr_) {
    other.owning_ = false;
    other.ptr_ = nullptr;
  }

  MaybeOwning& operator=(MaybeOwning&& other) noexcept {
    if (&other != this) {
      Reset();
      std::swap(owning_, other.owning_);
      std::swap(ptr_, other.ptr_);
    }
    return *this;
  }

  // Not copyable.
  MaybeOwning(const MaybeOwning&) = delete;
  MaybeOwning& operator=(const MaybeOwning&) = delete;

  // The pointer is freed (if we own it) on destruction.
  ~MaybeOwning() {
    if (owning_) {
      delete ptr_;
    }
  }

  // Accessor.
  constexpr T* Get() const noexcept { return ptr_; }
  constexpr T* operator->() const noexcept { return Get(); }
  constexpr T& operator*() const noexcept { return *Get(); }

  // Test if we're holding a valid pointer.
  constexpr explicit operator bool() const noexcept { return ptr_; }

  // Test if we own the pointer.
  constexpr bool Owning() const noexcept { return owning_; }

  // Reset to empty pointer.
  constexpr MaybeOwning& operator=(std::nullptr_t) noexcept {
    Reset();
    return *this;
  }

  // Release the pointer (without freeing it) to the caller.
  //
  // If we're not owning the pointer, calling this method cause undefined
  // behavior. (Use `Get()` instead.)
  [[nodiscard]] constexpr T* Leak() noexcept {
    FLARE_CHECK(
        owning_,
        "Calling `Leak()` on non-owning `MaybeOwning<T>` is undefined.");
    owning_ = false;
    return std::exchange(ptr_, nullptr);
  }

  // Release what we currently hold and hold a new pointer.
  constexpr void Reset(owning_t, T* ptr) noexcept { Reset(ptr, true); }
  constexpr void Reset(non_owning_t, T* ptr) noexcept { Reset(ptr, false); }

  // I'm not sure if we want to provide an `operator std::unique_ptr<T>() &&`.
  // This is dangerous as we could not check for `Owning()` at compile time.

 private:
  FRIEND_TEST(MaybeOwning, Reset);

  // Reset the pointer we have.
  //
  // These two were public interfaces, but I think we'd better keep them private
  // as `x.Reset(..., false)` is not as obvious as `x =
  // MaybeOwning(non_owning, ...)`.
  constexpr void Reset() noexcept { return Reset(nullptr, false); }
  constexpr void Reset(T* ptr, bool owning) noexcept {
    if (owning_) {
      FLARE_CHECK(ptr_);
      delete ptr_;
    }
    FLARE_CHECK(
        !owning_ || ptr_,
        "Passing a `nullptr` to `ptr` while specifying `owning` as `true` does "
        "not make much sense, I think.");
    ptr_ = ptr;
    owning_ = owning;
  }

 private:
  template <class U>
  friend class MaybeOwning;

  // TODO(luobogao): Optimize this. Use most significant bit for storing
  // `owning_` instead. (But keep an eye on performance as doing this requires
  // every pointer access to do arithmetic to mask out the highest bit.)
  bool owning_;
  T* ptr_;
};

template <class T>
class MaybeOwning<T[]>;  // Not implemented (yet).

// When used as function argument, it can be annoying to explicitly constructing
// `MaybeOwning<T>` at every call-site. If it deemed acceptable to always treat
// raw pointer as non-owning (which, in most cases, you should), you can use
// this class as argument type instead.
template <class T>
class MaybeOwningArgument {
 public:
  template <class U, class = decltype(MaybeOwning<T>(std::declval<U&&>()))>
  /* implicit */ MaybeOwningArgument(U&& ptr) : ptr_(std::forward<U>(ptr)) {}

  template <class U, class = std::enable_if_t<
                         MaybeOwning<T>::template is_convertible_from_v<U>>>
  /* implicit */ MaybeOwningArgument(U* ptr) : ptr_(non_owning, ptr) {}

  operator MaybeOwning<T>() && noexcept { return std::move(ptr_); }

 private:
  MaybeOwning<T> ptr_;
};

}  // namespace flare

#endif  // FLARE_BASE_MAYBE_OWNING_H_
