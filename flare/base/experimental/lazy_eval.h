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

#ifndef FLARE_BASE_EXPERIMENTAL_LAZY_EVAL_H_
#define FLARE_BASE_EXPERIMENTAL_LAZY_EVAL_H_

#include <type_traits>
#include <utility>
#include <variant>

#include "flare/base/function.h"
#include "flare/base/logging.h"

namespace flare::experimental {

// This class helps you to defer evaluation until needed.
template <class T>
class LazyEval {
 public:
  LazyEval() = default;

  // Capture the value directly.
  template <class... Us,
            class = std::enable_if_t<std::is_constructible_v<T, Us&&...>>>
  /* implicit */ LazyEval(Us&&... args)
      : func_or_val_(std::forward<Us>(args)...) {}

  // Captures a functor that produces the desired value.
  template <class U, class = std::enable_if_t<std::is_invocable_r_v<T, U>>>
  /* implicit */ LazyEval(U&& u) : func_or_val_(std::forward<U>(u)) {}

  // Conversion between `LazyEval`s.
  //
  // FIXME: Capturing `from` would incur memory allocation, not sure if we can
  // optimize that away.
  template <class U, class = std::enable_if_t<std::is_convertible_v<U, T>>>
  /* implicit */ LazyEval(LazyEval<U>&& from)
      : func_or_val_(
            [from = std::move(from)]() mutable { return from.Evaluate(); }) {}

  // Evaluate the functor captured before (if we haven't done yet).
  T& Evaluate() {
    if (func_or_val_.index() == 0) {
      func_or_val_.template emplace<1>(std::get<0>(func_or_val_)());
    }
    return Get();
  }

  bool IsEvaluated() const noexcept {
    FLARE_CHECK(*this,
                "You may not call `IsEvaluated()` on a not-initialized "
                "`LazyEval` instance.");
    return func_or_val_.index() == 1;
  }

  // Get value. This method may not be called unless `IsEvaluated()` holds,
  // otherwise the behavior is undefined.
  T& Get() noexcept {
    FLARE_CHECK(IsEvaluated(), "`LazyEval::Get()` is on evaluate instance.");
    return std::get<1>(func_or_val_);
  }
  const T& Get() const noexcept {
    FLARE_CHECK(IsEvaluated(), "`LazyEval::Get()` is on evaluate instance.");
    return std::get<1>(func_or_val_);
  }

  // Tests if we're holding a functor or a value. (i.e., `Evaluate()` can be
  // safely called.)
  constexpr explicit operator bool() const noexcept {
    return func_or_val_.index() == 1 || !!std::get<0>(func_or_val_);
  }

 private:
  std::variant<Function<T()>, T> func_or_val_;
};

}  // namespace flare::experimental

#endif  // FLARE_BASE_EXPERIMENTAL_LAZY_EVAL_H_
