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

#ifndef FLARE_BASE_FUTURE_BOXED_H_
#define FLARE_BASE_FUTURE_BOXED_H_

#include <tuple>
#include <utility>
#include <variant>

#include "flare/base/future/basics.h"
#include "flare/base/internal/logging.h"

namespace flare::future {
namespace detail {

// Returns a default constructed T (= Boxed<...>) that could be used later
// as a placeholder to retrieve `Boxed<...>` from somewhere.
template <class T>
T RetrieveBoxed();  // T = Boxed<...>.
}  // namespace detail

struct box_values_t {
  explicit box_values_t() = default;
};
inline constexpr box_values_t box_values;

// `Boxed` holds the result of an asynchronous execution, analogous to
// `Try` in Facebook's Folly.
template <class... Ts>
class Boxed {
 public:
  static_assert(!types_contains_v<Types<Ts...>, void>);

  using value_type = std::tuple<Ts...>;

  // Constructs `Boxed<...>` from values.
  template <class... Us, class = std::enable_if_t<
                             std::is_constructible_v<value_type, Us&&...>>>
  explicit Boxed(box_values_t, Us&&... imms);

  // Conversion from compatible `Boxed<...>`s.
  template <class... Us, class = std::enable_if_t<std::is_constructible_v<
                             value_type, std::tuple<Us&&...>>>>
  /* implicit */ Boxed(Boxed<Us...> boxed);

  // `Get` returns different types in different cases due to implementation
  // limitations, unfortunately.
  //
  // - When `Ts...` is empty, `Get` returns `void`;
  // - When there's exactly one type in `Ts...`, `Get` returns (ref to) that
  //   type;
  // - Otherwise (there're at least two types in `Ts...`), `Get` returns
  //   ref to `std::tuple<Ts...>`.
  //
  // There's a reason why we're specializing the case `sizeof...(Ts)` < 2:
  //
  // Once Coroutine TS is implemented, expression `co_await future;`
  // intuitively should be typed `void` or `T` in case there's none or one
  // type in `Ts...`, returning `std::tuple<>` or `std::tuple<T>` needlessly
  // complicated the user's life.
  //
  // (Indeed it's technically possible for us to specialize the type of the
  // `co_await` expression in `operator co_await` instead, but...)
  //
  // Another reason why we're specializing the case is that this is the
  // majority of use cases. Removing needless `std::tuple` ease our user
  // a lot even if it's an inconsistency.
  //
  // Also note that in case there're more than one types in `Ts`, the user
  // might leverage structured binding to ease their life with `auto&& [x, y]
  // = boxed.Get();`
  //
  // Returns: See above.
  std::add_lvalue_reference_t<unboxed_type_t<Ts...>> Get() &;
  std::add_rvalue_reference_t<unboxed_type_t<Ts...>> Get() &&;

  // `GetRaw` returns the `std::tuple` we're holding.
  value_type& GetRaw() &;
  value_type&& GetRaw() &&;

 private:
  template <class T>
  friend T detail::RetrieveBoxed();
  template <class... Us>
  friend class Boxed;

  // Indices in `holding_`.
  static constexpr std::size_t kEmpty = 0;
  static constexpr std::size_t kValue = 1;

  // A default constructed one is of little use except for being a placeholder.
  // None of the members (except for move assignment) may be called on a
  // default constructed `Boxed<...>`.
  //
  // For internal use only.
  Boxed() = default;

  std::variant<std::monostate, value_type> holding_;
};

static_assert(
    std::is_constructible_v<Boxed<int, double>, box_values_t, int, char>);
static_assert(
    std::is_constructible_v<Boxed<int, double>, Boxed<double, float>>);

// Implementation goes below.

namespace detail {

template <class T>
T RetrieveBoxed() {
  return T();
}

}  // namespace detail

template <class... Ts>
template <class... Us, class>
Boxed<Ts...>::Boxed(box_values_t, Us&&... imms)
    : holding_(std::in_place_index<kValue>, std::forward<Us>(imms)...) {}

template <class... Ts>
template <class... Us, class>
Boxed<Ts...>::Boxed(Boxed<Us...> boxed) {
  holding_.template emplace<kValue>(
      std::get<kValue>(std::move(boxed).holding_));
}

template <class... Ts>
std::add_lvalue_reference_t<unboxed_type_t<Ts...>> Boxed<Ts...>::Get() & {
  FLARE_CHECK_NE(holding_.index(), kEmpty);
  if constexpr (sizeof...(Ts) == 0) {
    return (void)GetRaw();
  } else if constexpr (sizeof...(Ts) == 1) {
    return std::get<0>(GetRaw());
  } else {
    return GetRaw();
  }
}

template <class... Ts>
std::add_rvalue_reference_t<unboxed_type_t<Ts...>> Boxed<Ts...>::Get() && {
  if constexpr (std::is_void_v<decltype(Get())>) {
    return Get();  // Nothing to move then.
  } else {
    return std::move(Get());
  }
}

template <class... Ts>
typename Boxed<Ts...>::value_type& Boxed<Ts...>::GetRaw() & {
  FLARE_CHECK_NE(holding_.index(), kEmpty);
  return std::get<kValue>(holding_);
}

template <class... Ts>
typename Boxed<Ts...>::value_type&& Boxed<Ts...>::GetRaw() && {
  return std::move(GetRaw());
}

}  // namespace flare::future

#endif  // FLARE_BASE_FUTURE_BOXED_H_
