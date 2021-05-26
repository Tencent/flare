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
//
// Type-erasing wrapper for `Callable`s.
//
// INTENDED FOR USE IN FUNCTION PARAMETER, **ONLY**.
//
// `Callable`s are neither required to be `CopyConstructible` nor
// `MoveConstructible`. They're not copy / moved at all.
//
// When applicable, this wrapper is more efficient than `std::function` /
// `Function`.
//
// BE AWARE, `FUNCTOR` USED TO CONSTRUCT `FUNCTIONVIEW` IS EXPECTED TO LIVE
// THROUGH `FUNCTIONVIEW`'S LIFETIME.
//
// Normally this is the case if `FunctionView` is used as function parameter and
// is not leaked outside of the function. The spec explicitly requires all
// temporaries created in calling function (including those for constructing
// `FunctionView`) shall be alive until the function-call expression ends.

#ifndef FLARE_BASE_FUNCTION_VIEW_H_
#define FLARE_BASE_FUNCTION_VIEW_H_

#include <functional>
#include <type_traits>
#include <utility>

namespace flare {

template <class T>
class FunctionView;

// Deduction of `noexcept` is not part of C++17, actually.
//
// It's an extension: https://gcc.gnu.org/ml/gcc-patches/2016-11/msg00665.html
template <class R, class... Args, bool kNoexcept>
class FunctionView<R(Args...) noexcept(kNoexcept)> {
 public:
  // Same as the constraints in `Function`.
  //
  // `constexpr auto` doesn't work here since Clang 9. LLVM Bug 43459 seems
  // related: https://bugs.llvm.org/show_bug.cgi?id=43459 .
  template <class T>
  static constexpr bool implicitly_convertible_v =
      std::is_invocable_r_v<R, T, Args...> &&
      !std::is_same_v<std::decay_t<T>, FunctionView> &&
      (!kNoexcept || std::is_nothrow_invocable_v<T, Args...>);

  // Default construction is not supported. I'm not sure if it's of any use.
  //
  // Copy & move is trivial, if the user wants to. (Although I'm not sure if
  // they can do it correctly. This is as dangerous as copying / moving
  // `std::string_view`.).

  // Construct `FunctionView` by referring to a `functor`.
  template <class T, class = std::enable_if_t<implicitly_convertible_v<T>>>
  constexpr /* implicit */ FunctionView(T&& functor) {
    using RefRemoved = std::remove_reference_t<T>;
    constexpr auto kConstantFunctor = std::is_const_v<RefRemoved>;

    // `functor` is casted to `const ...` unconditionally to simplify coding.
    //
    // We'll strip the `const` if appropriate in `invoker_`.
    object_ = reinterpret_cast<const void*>(&functor);
    invoker_ = [](const void* object, Args&&... args) -> R {
      if constexpr (kConstantFunctor) {
        return Invoke(std::forward<T>(*reinterpret_cast<RefRemoved*>(object)),
                      std::forward<Args>(args)...);
      } else {
        // Otherwise we need to strip `const` we've added to `object_`.
        return Invoke(std::forward<T>(*reinterpret_cast<RefRemoved*>(
                          const_cast<void*>(object))),
                      std::forward<Args>(args)...);
      }
    };
  }

  // Call the functor used to construct us.
  R operator()(Args... args) const noexcept(kNoexcept) {
    return invoker_(object_, std::forward<Args>(args)...);
  }

 private:
  using Invoker = R (*)(const void* object, Args&&... args);

  // Helper method for calling `functor`. Specializes for case where `R` is
  // `void`.
  template <class T>
  static R Invoke(T&& functor, Args&&... args) noexcept(kNoexcept) {
    if constexpr (std::is_void_v<R>) {
      std::invoke(std::forward<T>(functor), std::forward<Args>(args)...);
    } else {
      return std::invoke(std::forward<T>(functor), std::forward<Args>(args)...);
    }
  }

  const void* object_;
  Invoker invoker_;
};

// Deduction guides are not provided. As the template parameter is always
// required when declaring function parameter's type.
}  // namespace flare

#endif  // FLARE_BASE_FUNCTION_VIEW_H_
