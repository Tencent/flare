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

#ifndef FLARE_BASE_FUTURE_FUTURE_IMPL_H_
#define FLARE_BASE_FUTURE_FUTURE_IMPL_H_

// clang-format off
#include "flare/base/future/future.h"
// clang-format on

#include <memory>
#include <tuple>
#include <utility>

#include "flare/base/internal/logging.h"

namespace flare::future {

template <class... Ts>
template <class... Us, class>
Future<Ts...>::Future(futurize_values_t, Us&&... imms)
    : core_(std::make_shared<Core<Ts...>>(GetDefaultExecutor())) {
  core_->SetBoxed(Boxed<Ts...>(box_values, std::forward<Us>(imms)...));
}

template <class... Ts>
template <class... Us, class>
Future<Ts...>::Future(futurize_tuple_t, std::tuple<Us...> values)
    : core_(std::make_shared<Core<Ts...>>(GetDefaultExecutor())) {
  core_->SetBoxed(Boxed<Ts...>(box_values, std::move(values)));
}

template <class... Ts>
template <class U, class>
Future<Ts...>::Future(U&& value)
    : core_(std::make_shared<Core<Ts...>>(GetDefaultExecutor())) {
  static_assert(sizeof...(Ts) == 1);
  core_->SetBoxed(Boxed<Ts...>(box_values, std::forward<U>(value)));
}

template <class... Ts>
template <class... Us, class>
Future<Ts...>::Future(Future<Us...>&& future) {
  Promise<Ts...> p;

  // Here we "steal" `p.GetFuture()`'s core and "install" it into ourself,
  // thus once `p` is satisfied, we're satisfied as well.
  core_ = p.GetFuture().core_;
  std::move(future).Then([p = std::move(p)](Boxed<Us...> boxed) mutable {
    p.SetBoxed(std::move(boxed));
  });
}

template <class... Ts>
template <class F, class R>
R Future<Ts...>::Then(F&& continuation) && {
  FLARE_CHECK(core_ != nullptr,
              "Calling `Then` on uninitialized `Future` is undefined.");

  // Evaluates to `true` if `F` can be called with `Ts...`.
  //
  // *It does NOT mean `F` cannot be called with `Boxed<Ts>...` even if
  // this is true*, e.g.:
  //
  //    `f.Then([] (auto&&...) {})`
  //
  // But it's technically impossible for us to tell whether the user is
  // expecting `Ts...` or `Boxed<Ts>...` by checking `F`'s signature.
  //
  // In case there's an ambiguity, we prefer `Ts...`, and let the users do
  // disambiguation if this is not what they mean.
  constexpr auto kCallUnboxed = std::is_invocable_v<F, Ts...>;

  using NewType =
      typename std::conditional_t<kCallUnboxed, std::invoke_result<F, Ts...>,
                                  std::invoke_result<F, Boxed<Ts...>>>::type;
  using NewFuture = futurize_t<NewType>;
  using NewBoxed =
      std::conditional_t<std::is_void_v<NewType>, Boxed<>, Boxed<NewType>>;

  as_promise_t<NewFuture> p(core_->GetExecutor());  // Propagate the executor.
  auto result = p.GetFuture();

  auto raw_cont = [p = std::move(p), cont = std::forward<F>(continuation)](
                      Boxed<Ts...>&& value) mutable noexcept {
    // The "boxed" value we get from calling `cont`.
    auto next_value = [&]() noexcept {
      // Specializes the cases for `kCallUnboxed` being true / false.
      auto next = [&] {
        if constexpr (kCallUnboxed) {
          return std::apply(cont, std::move(value.GetRaw()));
        } else {
          return std::invoke(cont, std::move(value));
        }
      };
      static_assert(std::is_same_v<NewType, decltype(next())>);

      if constexpr (std::is_void_v<NewType>) {
        next();
        return NewBoxed(box_values);
      } else {
        return NewBoxed(box_values, next());
      }
    }();

    // Unless we get a `Future` from the continuation, we can now satisfy
    // the promise we made.
    if constexpr (!is_future_v<NewType>) {
      p.SetBoxed(std::move(next_value));
    } else {  // We do get a new `Future`.
      // Forward the value in the `Future` we got to the promise we made.
      next_value.Get().core_->ChainAction(
          [p = std::move(p)](auto&& nested_v) mutable noexcept {
            static_assert(
                std::is_same_v<as_boxed_t<NewFuture>,
                               std::remove_reference_t<decltype(nested_v)>>);
            p.SetBoxed(std::move(nested_v));
          });
    }
  };

  core_->ChainAction(std::move(raw_cont));

  return result;
}

}  // namespace flare::future

#endif  // FLARE_BASE_FUTURE_FUTURE_IMPL_H_
