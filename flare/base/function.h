// Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
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
//
// Another mimic of `std::function<...>`, without requiring `Callable`
// to be CopyConstructible.

#ifndef FLARE_BASE_FUNCTION_H_
#define FLARE_BASE_FUNCTION_H_

#include <cstddef>

#include <functional>
#include <new>
#include <utility>

#include "flare/base/align.h"
#include "flare/base/likely.h"

namespace flare {

// Note that although some of the implementation of `std::function<...>`
// (including libstdc++, but not libc++, as of 20181126) is
// `TriviallyRelocatable`, ours is not.

template <class R>
class Function;

// This extension is only implemented by GCC / clang.
// @sa: https://bugs.llvm.org/show_bug.cgi?id=32974
#if defined(__GNUC__) || defined(__clang__)
template <class R, class... Args, bool kNoexcept>
class alignas(max_align_v) Function<R(Args...) noexcept(kNoexcept)> {
#else
template <class R, class... Args>
class alignas(max_align_v) Function<R(Args...)> {
  static const bool kNoexcept = false;  // private.
#endif

 public:
  // Evaluates to `true` if `T` should be implicitly convertible to us according
  // to the following rules:
  //
  // 1) `T` is invocable with arguments of type `Args...` and returns a type
  //    that is convertible to `R`;
  // 2) `T` (decayed) is not the same as us;
  // 3)  It's not a cast from a throwing signature to a `noexcept` one.
  //
  // N.b.: With hindsight, #3 seems to be an overkill as it's somewhat annoying
  //       to mark every functor as `noexcept` once we mark a prototype as
  //       accepting a `Function<... noexcept>`. I'm not sure if we want to do
  //       this (or if we want to support `noexcept` at all.).
  //
  //       FWIW, it looks like Facebook's Folly supports `noexcept`
  //       conditionally (by `FOLLY_HAVE_NOEXCEPT_FUNCTION_TYPE`) without
  //       enforcing #3.
  //
  //       Also note that C++'s type system do enforce #3 when casting between
  //       (non-)throwing function pointers.
  template <class T>
  static constexpr bool implicitly_convertible_v =
      std::is_invocable_r_v<R, T, Args...> &&                   // #1
      !std::is_same_v<std::decay_t<T>, Function> &&             // #2
      (!kNoexcept || std::is_nothrow_invocable_v<T, Args...>);  // #3.

  // DefaultConstructible.
  constexpr Function() = default;

  // Construct an empty `Function`
  /* implicit */ Function(std::nullptr_t) {}  // `ops_` is initialized inline
                                              // as nullptr.

  // Forward `action` into `Function`.
  template <class T, class = std::enable_if_t<implicitly_convertible_v<T>>>
  /* implicit */ Function(T&& action);

  // Move constructor.
  Function(Function&& function) noexcept {
    ops_ = std::exchange(function.ops_, nullptr);
    if (ops_) {
      ops_->relocator(&object_, &function.object_);
    }
  }

  // Destructor.
  ~Function() {
    if (ops_) {
      ops_->destroyer(&object_);
    }
  }

  // Forward `action` into `Function`.
  template <class T, class = std::enable_if_t<implicitly_convertible_v<T>>>
  Function& operator=(T&& action) {
    this->~Function();
    new (this) Function(std::forward<T>(action));
    return *this;
  }

  // Move assignment.
  Function& operator=(Function&& function) noexcept {
    if (FLARE_LIKELY(&function != this)) {
      this->~Function();
      new (this) Function(std::move(function));
    }
    return *this;
  }

  // Reset `Function` to an empty one.
  Function& operator=(std::nullptr_t) {
    if (auto ops = std::exchange(ops_, nullptr)) {
      ops->destroyer(&object_);
    }
    return *this;
  }

  // Call functor stored inside.
  //
  // The behavior is undefined if `*this == nullptr` holds.
  R operator()(Args... args) const noexcept(kNoexcept) {
    // It's undefined to call upon a null `Function`, thus even if `ops_`
    // is `nullptr`, we're "well-behaving".
    return ops_->invoker(&object_, std::forward<Args>(args)...);
  }

  // Test if `Function` is empty.
  constexpr explicit operator bool() const { return !!ops_; }

 private:
  // Functors of size no greater than `kMaximumOptimizableSize` is stored
  // inplace inside `Function`.
  static constexpr std::size_t kMaximumOptimizableSize = 3 * sizeof(void*);

  struct TypeOps {  // One instance per type.
    using Invoker = R (*)(void* object, Args&&... args);
    using Relocator = void (*)(void* to, void* from);  // Move construction
                                                       // onto bytes, and
                                                       // destroy the source.
    using Destroyer = void (*)(void* object);

    Invoker invoker;
    Relocator relocator;
    Destroyer destroyer;
  };

  // If `R` is specified as `void`, we should discard the value the `object`
  // returned.
  //
  // This method accomplishes this.
  //
  // Simply casting `std::invoke(...)` to `R` does compile, but that incurs
  // a copy construction for non-trivial `R`s.
  template <class T>
  static R Invoke(T&& object, Args&&... args) {
    if constexpr (std::is_void_v<R>) {
      std::invoke(std::forward<T>(object), std::forward<Args>(args)...);
    } else {
      return std::invoke(std::forward<T>(object), std::forward<Args>(args)...);
    }
  }

  // Copy `object` to `buffer`, with type erased.
  //
  // `buffer` is expected to be at least `kMaximumOptimizableSize` bytes large.
  // `T` is expected to be no larger than `buffer`.
  //
  // Returns a pointer to ops that can be used to manipulate the `buffer`.
  template <class T>
  const TypeOps* ErasedCopySmall(void* buffer, T&& object) {
    using Decayed = std::decay_t<T>;

    static constexpr TypeOps ops = {
        /* invoker */ [](void* object, Args&&... args) -> R {
          return Invoke(*static_cast<Decayed*>(object),
                        std::forward<Args>(args)...);
        },
        /* relocator */
        [](void* to, void* from) {
          new (to) Decayed(std::move(*static_cast<Decayed*>(from)));
          static_cast<Decayed*>(from)->~Decayed();
        },
        /* destroyer */
        [](void* object) {
          // Object was constructed in place, only dtor is called, memory is not
          // freed as there was no allocation.
          static_cast<Decayed*>(object)->~Decayed();
        }};

    new (buffer) Decayed(std::forward<T>(object));
    return &ops;
  }

  // Same as `ErasedCopySmall`, except that there's size requirement for `T`.
  template <class T>
  const TypeOps* ErasedCopyLarge(void* buffer, T&& object) {
    using Decayed = std::decay_t<T>;
    using Stored = Decayed*;

    static constexpr TypeOps ops = {
        /* invoker */ [](void* object, Args&&... args) -> R {
          return Invoke(**static_cast<Stored*>(object),
                        std::forward<Args>(args)...);
        },
        /* relocator */
        [](void* to, void* from) {
          // We're copying pointer here.
          new (to) Stored(*static_cast<Stored*>(from));
          // Since `Stored` is simply a pointer, there's nothing for us to
          // destroy.
        },
        /* destroyer */
        [](void* object) {
          delete *static_cast<Stored*>(object);
          // `Stored` it self needs no destruction.
        }};

    new (buffer) Stored(new Decayed(std::forward<T>(object)));  // Pointer is
                                                                // stored.
    return &ops;
  }

  // Pointer is stored if the object is too large to be stored in-place.
  //
  // Alignment is taken care of by `Function<T>` itself.
  mutable std::aligned_storage_t<kMaximumOptimizableSize, 1> object_;
  const TypeOps* ops_ = nullptr;  // `nullptr` if `Function` does not contain
                                  // a valid functor.
};

// Deduction guides.
namespace details {

template <class TSignature>
struct FunctionTypeDeducer;
template <class T>
using FunctionSignature = typename FunctionTypeDeducer<T>::Type;

}  // namespace details

#if defined(__GNUC__) || defined(__clang__)
template <class R, class... Args, bool kNoexcept>
Function(R (*)(Args...) noexcept(kNoexcept))
    -> Function<R(Args...) noexcept(kNoexcept)>;
#else
template <class R, class... Args>
Function(R (*)(Args...)) -> Function<R(Args...)>;
#endif

template <class F, class Signature =
                       details::FunctionSignature<decltype(&F::operator())>>
Function(F) -> Function<Signature>;

// What about deduction guides for member methods? I don't see STL defined
// them.

// Comparison to `nullptr`.
template <class T>
bool operator==(const Function<T>& f, std::nullptr_t) {
  return !f;
}

template <class T>
bool operator==(std::nullptr_t, const Function<T>& f) {
  return !f;
}

template <class T>
bool operator!=(const Function<T>& f, std::nullptr_t) {
  return !(f == nullptr);
}

template <class T>
bool operator!=(std::nullptr_t, const Function<T>& f) {
  return !(nullptr == f);
}

// Implementation goes below.

#if defined(__GNUC__) || defined(__clang__)
template <class R, class... Args, bool kNoexcept>
template <class T, class>
Function<R(Args...) noexcept(kNoexcept)>::Function(T&& action) {
#else
template <class R, class... Args>
template <class T, class>
Function<R(Args...)>::Function(T&& action) {
#endif

  // There's still room for further optimization.
  //
  // In case `T` is a function pointer, we could completely skip the
  // `ErasedCopyXxx` and simply save the function pointer to `ops_.invoker` and
  // leave other ops as noop.

  if (sizeof(std::decay_t<T>) <= kMaximumOptimizableSize) {
    ops_ = ErasedCopySmall(&object_, std::forward<T>(action));
  } else {
    // TODO(luobogao): Consider using our object pool to allocate memory for
    // functors smaller than 128 bytes.
    ops_ = ErasedCopyLarge(&object_, std::forward<T>(action));
  }
}

namespace details {

#if defined(__GNUC__) || defined(__clang__)

template <class R, class Class, class... Args, bool kNoexcept>
struct FunctionTypeDeducer<R (Class::*)(Args...) noexcept(kNoexcept)> {
  using Type = R(Args...) noexcept(kNoexcept);
};

template <class R, class Class, class... Args, bool kNoexcept>
struct FunctionTypeDeducer<R (Class::*)(Args...) & noexcept(kNoexcept)> {
  using Type = R(Args...) noexcept(kNoexcept);
};

template <class R, class Class, class... Args, bool kNoexcept>
struct FunctionTypeDeducer<R (Class::*)(Args...) const noexcept(kNoexcept)> {
  using Type = R(Args...) noexcept(kNoexcept);
};

template <class R, class Class, class... Args, bool kNoexcept>
struct FunctionTypeDeducer<R (Class::*)(Args...) const & noexcept(kNoexcept)> {
  using Type = R(Args...) noexcept(kNoexcept);
};

#else

template <class R, class Class, class... Args>
struct FunctionTypeDeducer<R (Class::*)(Args...)> {
  using Type = R(Args...);
};

template <class R, class Class, class... Args>
struct FunctionTypeDeducer<R (Class::*)(Args...)&> {
  using Type = R(Args...);
};

template <class R, class Class, class... Args>
struct FunctionTypeDeducer<R (Class::*)(Args...) const> {
  using Type = R(Args...);
};

template <class R, class Class, class... Args>
struct FunctionTypeDeducer<R (Class::*)(Args...) const&> {
  using Type = R(Args...);
};

#endif

}  // namespace details

}  // namespace flare

#endif  // FLARE_BASE_FUNCTION_H_
