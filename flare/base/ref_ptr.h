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

#ifndef FLARE_BASE_REF_PTR_H_
#define FLARE_BASE_REF_PTR_H_

#include <atomic>
#include <utility>

#include "flare/base/internal/logging.h"

namespace flare {

// @sa: Constructor of `RefPtr`.
constexpr struct ref_ptr_t {
  explicit ref_ptr_t() = default;
} ref_ptr;
constexpr struct adopt_ptr_t {
  explicit adopt_ptr_t() = default;
} adopt_ptr;

// You need to specialize this traits unless your type is inherited from
// `RefCounted<T>`.
template <class T, class = void>
struct RefTraits {
  // Increment reference counter on `ptr` with `std::memory_order_relaxed`.
  static void Reference(T* ptr) noexcept {
    static_assert(
        sizeof(T) == 0,
        "To use `RefPtr<T>`, you need either inherit from `RefCounted<T>` "
        "(which you didn't), or specialize `RefTraits<T>`.");
  }

  // Decrement reference counter on `ptr` with `std::memory_order_acq_rel`.
  //
  // If the counter after decrement reaches zero, resource allocated for `ptr`
  // should be released.
  static void Dereference(T* ptr) noexcept {
    static_assert(
        sizeof(T) == 0,
        "To use `RefPtr<T>`, you need either inherit from `RefCounted<T>` "
        "(which you didn't), or specialize `RefTraits<T>`.");
  }
};

// For types inherited from `RefCounted`, they're supported by `RefPtr` (or
// `RefTraits<T>`, to be accurate) by default.
//
// Default constructed one has a reference count of one (every object whose
// reference count reaches zero should have already been destroyed). Should you
// want to construct a `RefPtr`, use overload that accepts `adopt_ptr`.
//
// IMPLEMENTATION DETAIL: For the sake of implementation simplicity, at the
// moment `Deleter` is declared as a friend to `RefCounted<T>`. This declaration
// is subject to change. UNLESS YOU'RE A DEVELOPER OF FLARE, YOU MUST NOT RELY
// ON THIS.
template <class T, class Deleter = std::default_delete<T>>
class RefCounted {
 public:
  // Increment ref-count.
  constexpr void Ref() noexcept;

  // Decrement ref-count, if it reaches zero after the decrement, the pointer is
  // freed by `Deleter`.
  constexpr void Deref() noexcept;

  // Get current ref-count.
  //
  // It's unsafe, as by the time the ref-count is returned, it may well have
  // changed. The only return value that you can rely on is 1, which means no
  // one else is referencing this object.
  constexpr std::uint32_t UnsafeRefCount() const noexcept;

 protected:
  // `RefCounted` must be inherited.
  RefCounted() = default;

  // Destructor is NOT defined as virtual on purpose, we always cast `this` to
  // `T*` before calling `delete`.

 private:
  friend Deleter;

  // Hopefully `std::uint32_t` is large enough to store a ref count.
  //
  // We tried using `std::uint_fast32_t` here, it uses 8 bytes. I'm not aware of
  // the benefit of using an 8-byte integer here (at least for the purpose of
  // reference-couting.)
  std::atomic<std::uint32_t> ref_count_{1};
};

// Keep the overhead the same as an atomic `std::uint32_t`.
static_assert(sizeof(RefCounted<int>) == sizeof(std::atomic<std::uint32_t>));

// Utilities for internal use.
//
// FIXME: I'm not sure if it compiles quickly enough, but it might worth a look.
namespace detail {

// This methods serves multiple purposes:
//
// - If `T` is explicitly specified, it tests if `T` inherits from
//   `RefCounted<T, ...>`, and returns `RefCounted<T, ...>`;
//
// - If `T` is not specified, it tests if `T` inherits from some `RefCounted<U,
//   ...>`, and returns `RefCounted<U, ...>`;
//
// - Otherwise `int` is returned, which can never inherits from `RefCounted<int,
//   ...>`.
template <class T, class... Us>
RefCounted<T, Us...> GetRefCountedType(const RefCounted<T, Us...>*);
template <class T>
int GetRefCountedType(...);
int GetRefCountedType(...);  // Match failure.

// Extract `T` from `RefCounted<T, ...>`.
//
// We're returning `std::common_type<T>` instead of `T` so as not to require `T`
// to be complete.
template <class T, class... Us>
std::common_type<T> GetRefeeType(const RefCounted<T, Us...>*);
std::common_type<int> GetRefeeType(...);

template <class T>
using refee_t =
    typename decltype(GetRefeeType(reinterpret_cast<const T*>(0)))::type;

// If `T` inherits from `RefCounted<T, ...>` directly, `T` is returned,
// otherwise `int` (which can never inherits `RefCounted<int, ...>`) is
// returned.
template <class T>
using direct_refee_t =
    refee_t<decltype(GetRefCountedType<T>(reinterpret_cast<const T*>(0)))>;

// Test if `T` is a subclass of some `RefCounted<T, ...>`.
template <class T>
constexpr auto is_ref_counted_directly_v =
    !std::is_same_v<int, direct_refee_t<T>>;

// If `T` is inheriting from some `RefCounted<U, ...>` unambiguously, this
// method returns `U`. Otherwise it returns `int`.
template <class T>
using indirect_refee_t =
    refee_t<decltype(GetRefCountedType(reinterpret_cast<const T*>(0)))>;

// Test if:
//
// - `T` is inherited from `RefCounted<U, ...>` indirectly;  (1)
// - `U` is inherited from `RefCounted<U, ...>` directly;    (2)
// - and `U` has a virtual destructor.                       (3)
//
// In this case, we can safely use `RefTraits<RefCounted<U, ...>>` on
// `RefCounted<T>`.
template <class T>
constexpr auto is_ref_counted_indirectly_safe_v =
    !is_ref_counted_directly_v<T> &&                     // 1
    is_ref_counted_directly_v<indirect_refee_t<T>> &&    // 2
    std::has_virtual_destructor_v<indirect_refee_t<T>>;  // 3

// Test if `RefTraits<as_ref_counted_t<T>>` is safe for `T`.
//
// We consider default traits as safe if either:
//
// - `T` is directly inheriting `RefCounted<T, ...>` (so its destructor does not
//   have to be `virtual`), or
// - `T` inherts from `RefCounted<U, ...>` and `U`'s destructor is virtual.
template <class T>
constexpr auto is_default_ref_traits_safe_v =
    detail::is_ref_counted_directly_v<T> ||
    detail::is_ref_counted_indirectly_safe_v<T>;

template <class T>
using as_ref_counted_t =
    decltype(GetRefCountedType(reinterpret_cast<const T*>(0)));

}  // namespace detail

// Specialization for `RefCounted<T>` ...
template <class T, class Deleter>
struct RefTraits<RefCounted<T, Deleter>> {
  static void Reference(RefCounted<T, Deleter>* ptr) noexcept {
    FLARE_DCHECK_GT(ptr->UnsafeRefCount(), 0);
    ptr->Ref();
  }

  static void Dereference(RefCounted<T, Deleter>* ptr) noexcept {
    FLARE_DCHECK_GT(ptr->UnsafeRefCount(), 0);
    ptr->Deref();
  }
};

// ... and its subclasses.
template <class T>
struct RefTraits<
    T, std::enable_if_t<!std::is_same_v<detail::as_ref_counted_t<T>, T> &&
                        detail::is_default_ref_traits_safe_v<T>>>
    : RefTraits<detail::as_ref_counted_t<T>> {};

// Our own design of `retain_ptr`, with several naming changes to make it easier
// to understand.
//
// @sa: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0468r1.html
template <class T>
class RefPtr final {
  using Traits = RefTraits<T>;

 public:
  // Default constructed one does not own a pointer.
  constexpr RefPtr() noexcept : ptr_(nullptr) {}

  // Same as default-constructed one.
  /* implicit */ constexpr RefPtr(std::nullptr_t) noexcept : ptr_(nullptr) {}

  // Reference counter is decrement on destruction.
  ~RefPtr() {
    if (ptr_) {
      Traits::Dereference(ptr_);
    }
  }

  // Increment reference counter on `ptr` (if it's not `nullptr`) and hold it.
  //
  // P0468 declares `xxx_t` as the second argument, but I'd still prefer to
  // place it as the first one. `std::scoped_lock` revealed the shortcoming of
  // placing `std::adopt_lock_t` as the last parameter, I won't repeat that
  // error here.
  constexpr RefPtr(ref_ptr_t, T* ptr) noexcept;

  // Hold `ptr` without increasing its reference counter.
  constexpr RefPtr(adopt_ptr_t, T* ptr) noexcept : ptr_(ptr) {}

  // TBH this is a dangerous conversion constructor. Even if `T` does not have
  // virtual destructor.
  //
  // However, testing if `T` has a virtual destructor comes with a price: we'll
  // require `T` to be complete when defining `RefPtr<T>`, which is rather
  // annoying.
  //
  // Given that `std::unique_ptr` does not test destructor's virtualization
  // state either, we ignore it for now.
  template <class U, class = std::enable_if_t<std::is_convertible_v<U*, T*>>>
  /* implicit */ constexpr RefPtr(RefPtr<U> ptr) noexcept : ptr_(ptr.Leak()) {}

  // Copyable, movable.
  constexpr RefPtr(const RefPtr& ptr) noexcept;
  constexpr RefPtr(RefPtr&& ptr) noexcept;
  constexpr RefPtr& operator=(const RefPtr& ptr) noexcept;
  constexpr RefPtr& operator=(RefPtr&& ptr) noexcept;

  // `boost::intrusive_ptr(T*)` increments reference counter, while
  // `std::retained_ptr(T*)` (as specified by the proposal) does not. The
  // inconsistency between them can be confusing. To be perfect clear, we do not
  // support `RefPtr(T*)`.
  //
  // RefPtr(T*) = delete;

  // Accessors.
  constexpr T* operator->() const noexcept { return Get(); }
  constexpr T& operator*() const noexcept { return *Get(); }
  constexpr T* Get() const noexcept { return ptr_; }

  // Test if *this holds a pointer.
  constexpr explicit operator bool() const noexcept { return ptr_; }

  // Equivalent to `Reset()`.
  constexpr RefPtr& operator=(std::nullptr_t) noexcept;

  // Reset *this to an empty one.
  constexpr void Reset() noexcept;

  // Release whatever *this currently holds and hold `ptr` instead.
  constexpr void Reset(ref_ptr_t, T* ptr) noexcept;
  constexpr void Reset(adopt_ptr_t, T* ptr) noexcept;

  // Gives up ownership on its internal pointer, which is returned.
  //
  // The caller is responsible for calling `RefTraits<T>::Dereference` when it
  // sees fit.
  [[nodiscard]] constexpr T* Leak() noexcept {
    return std::exchange(ptr_, nullptr);
  }

 private:
  T* ptr_;
};

// Shorthand for `new`-ing an object and **adopt** (NOT *ref*, i.e., the
// initial ref-count must be 1) it into a `RefPtr`.
template <class T, class... Us>
RefPtr<T> MakeRefCounted(Us&&... args) {
  return RefPtr(adopt_ptr, new T(std::forward<Us>(args)...));
}

template <class T, class Deleter>
constexpr void RefCounted<T, Deleter>::Ref() noexcept {
  auto was = ref_count_.fetch_add(1, std::memory_order_relaxed);
  FLARE_DCHECK_GT(was, 0);
}

template <class T, class Deleter>
constexpr void RefCounted<T, Deleter>::Deref() noexcept {
  // It seems that we can simply test if `ref_count_` is 1, and save an atomic
  // operation if it is (as we're the only reference holder). However I don't
  // see a perf. boost in implementing it, so I keep it unchanged. we might want
  // to take a deeper look later.
  if (auto was = ref_count_.fetch_sub(1, std::memory_order_acq_rel); was == 1) {
    Deleter()(static_cast<T*>(this));  // Hmmm.
  } else {
    FLARE_CHECK_GT(was, 1);
  }
}

template <class T, class Deleter>
constexpr std::uint32_t RefCounted<T, Deleter>::UnsafeRefCount()
    const noexcept {
  return ref_count_.load(std::memory_order_relaxed);  // FIXME: `acquire`?
}

template <class T>
constexpr RefPtr<T>::RefPtr(ref_ptr_t, T* ptr) noexcept : ptr_(ptr) {
  if (ptr) {
    Traits::Reference(ptr_);
  }
}

template <class T>
constexpr RefPtr<T>::RefPtr(const RefPtr& ptr) noexcept : ptr_(ptr.ptr_) {
  if (ptr_) {
    Traits::Reference(ptr_);
  }
}

template <class T>
constexpr RefPtr<T>::RefPtr(RefPtr&& ptr) noexcept : ptr_(ptr.ptr_) {
  ptr.ptr_ = nullptr;
}

template <class T>
constexpr RefPtr<T>& RefPtr<T>::operator=(const RefPtr& ptr) noexcept {
  if (FLARE_UNLIKELY(&ptr == this)) {
    return *this;
  }
  if (ptr_) {
    Traits::Dereference(ptr_);
  }
  ptr_ = ptr.ptr_;
  if (ptr_) {
    Traits::Reference(ptr_);
  }
  return *this;
}

template <class T>
constexpr RefPtr<T>& RefPtr<T>::operator=(RefPtr&& ptr) noexcept {
  if (FLARE_UNLIKELY(&ptr == this)) {
    return *this;
  }
  if (ptr_) {
    Traits::Dereference(ptr_);
  }
  ptr_ = ptr.ptr_;
  ptr.ptr_ = nullptr;
  return *this;
}

template <class T>
constexpr RefPtr<T>& RefPtr<T>::operator=(std::nullptr_t) noexcept {
  Reset();
  return *this;
}

template <class T>
constexpr void RefPtr<T>::Reset() noexcept {
  if (ptr_) {
    Traits::Dereference(ptr_);
    ptr_ = nullptr;
  }
}

template <class T>
constexpr void RefPtr<T>::Reset(ref_ptr_t, T* ptr) noexcept {
  Reset();
  ptr_ = ptr;
  Traits::Reference(ptr_);
}

template <class T>
constexpr void RefPtr<T>::Reset(adopt_ptr_t, T* ptr) noexcept {
  Reset();
  ptr_ = ptr;
}

// I'd rather implement `operator<=>` instead.
template <class T>
constexpr bool operator==(const RefPtr<T>& left,
                          const RefPtr<T>& right) noexcept {
  return left.Get() == right.Get();
}

template <class T>
constexpr bool operator==(const RefPtr<T>& ptr, std::nullptr_t) noexcept {
  return ptr.Get() == nullptr;
}

template <class T>
constexpr bool operator==(std::nullptr_t, const RefPtr<T>& ptr) noexcept {
  return ptr.Get() == nullptr;
}

}  // namespace flare

namespace std {

// Specialization for `std::atomic<flare::RefPtr<T>>`.
//
// @sa: https://en.cppreference.com/w/cpp/memory/shared_ptr/atomic2
template <class T>
class atomic<flare::RefPtr<T>> {
  using Traits = flare::RefTraits<T>;

 public:
  constexpr atomic() noexcept : ptr_(nullptr) {}

  // FIXME: It destructor of specialization of `std::atomic>` allowed to be
  // non-trivial? I don't see a way to implement a trival destructor while
  // maintaining correctness.
  ~atomic() {
    if (auto ptr = ptr_.load(std::memory_order_acquire)) {
      Traits::Dereference(ptr);
    }
  }

  // Receiving a pointer.
  constexpr /* implicit */ atomic(flare::RefPtr<T> ptr) noexcept
      : ptr_(ptr.Leak()) {}
  atomic& operator=(flare::RefPtr<T> ptr) noexcept {
    store(std::move(ptr));
    return *this;
  }

  // Tests if the implementation is lock-free.
  bool is_lock_free() const noexcept { return ptr_.is_lock_free(); }

  // Stores to this atomic ref-ptr.
  void store(flare::RefPtr<T> ptr,
             std::memory_order order = std::memory_order_seq_cst) noexcept {
    // Promoted to `exchange`, otherwise we can't atomically load current
    // pointer (to release it) and store a new one.
    exchange(std::move(ptr), order);
  }

  // Loads from this atomic ref-ptr.
  flare::RefPtr<T> load(
      std::memory_order order = std::memory_order_seq_cst) const noexcept {
    return flare::RefPtr<T>(flare::ref_ptr, ptr_.load(order));
  }

  // Same as `load()`.
  operator flare::RefPtr<T>() const noexcept { return load(); }

  // Exchanges with a (possibly) different ref-ptr.
  flare::RefPtr<T> exchange(
      flare::RefPtr<T> ptr,
      std::memory_order order = std::memory_order_seq_cst) noexcept {
    return flare::RefPtr(flare::adopt_ptr, ptr_.exchange(ptr.Leak(), order));
  }

  // Compares if this atomic holds the `expected` pointer, and exchanges it with
  // the new `desired` one if the comparsion holds.
  bool compare_exchange_strong(
      flare::RefPtr<T>& expected, flare::RefPtr<T> desired,
      std::memory_order order = std::memory_order_seq_cst) noexcept {
    return CompareExchangeImpl(
        [&](auto&&... args) { return ptr_.compare_exchange_strong(args...); },
        expected, std::move(desired), order);
  }
  bool compare_exchange_weak(
      flare::RefPtr<T>& expected, flare::RefPtr<T> desired,
      std::memory_order order = std::memory_order_seq_cst) noexcept {
    return CompareExchangeImpl(
        [&](auto&&... args) { return ptr_.compare_exchange_weak(args...); },
        expected, std::move(desired), order);
  }
  bool compare_exchange_strong(flare::RefPtr<T>& expected,
                               flare::RefPtr<T> desired,
                               std::memory_order success,
                               std::memory_order failure) noexcept {
    return CompareExchangeImpl(
        [&](auto&&... args) { return ptr_.compare_exchange_strong(args...); },
        expected, std::move(desired), success, failure);
  }
  bool compare_exchange_weak(flare::RefPtr<T>& expected,
                             flare::RefPtr<T> desired,
                             std::memory_order success,
                             std::memory_order failure) noexcept {
    return CompareExchangeImpl(
        [&](auto&&... args) { return ptr_.compare_exchange_weak(args...); },
        expected, std::move(desired), success, failure);
  }

#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L

  // Wait on this atomic.
  void wait(
      flare::RefPtr<T> old,
      std::memory_order order = std::memory_order_seq_cst) const noexcept {
    return ptr_.wait(old.Get(), order);
  }

  // Notifies one `wait`.
  void notify_one() noexcept { ptr_.notify_one(); }

  void notify_all() noexcept { ptr_.notify_all(); }

#endif

  // Not copyable, as requested by the Standard.
  atomic(const atomic&) = delete;
  atomic& operator=(const atomic&) = delete;

 private:
  template <class F, class... Orders>
  bool CompareExchangeImpl(F&& f, flare::RefPtr<T>& expected,
                           flare::RefPtr<T> desired, Orders... orders) {
    auto current = expected.Get();
    if (std::forward<F>(f)(current, desired.Get(), orders...)) {
      (void)desired.Leak();  // Ownership transfer to `ptr_`.
      // Ownership of the old pointer is transferred to us, release it.
      flare::RefPtr(flare::adopt_ptr, current);
      return true;
    }
    expected = load();  // FIXME: Promoted to `seq_cst` unnecessarily.
    return false;
  }

 private:
  std::atomic<T*> ptr_;  // We hold a reference to it.
};

}  // namespace std

#endif  // FLARE_BASE_REF_PTR_H_
