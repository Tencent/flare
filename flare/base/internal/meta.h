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

#ifndef FLARE_BASE_INTERNAL_META_H_
#define FLARE_BASE_INTERNAL_META_H_

#include <type_traits>
#include <utility>

namespace flare::internal {

// Some meta-programming helpers.

template <class F>
constexpr auto is_valid(F&& f) {
  return [f = std::forward<F>(f)](auto&&... args) constexpr {
    // FIXME: Perfect forwarding.
    return std::is_invocable_v<F, decltype(args)...>;
  };
}

// `x` should be used as placeholder's name in` expr`.
#define FLARE_INTERNAL_IS_VALID(expr) \
  ::flare::internal::is_valid([](auto&& x) -> decltype(expr) {})

// Clang's (as of Clang 10) `std::void_t` suffers from CWG 1558, to be able to
// use `std::void_t` in SFINAE, we roll our own one that immune to this issue.
//
// @sa: https://bugs.llvm.org/show_bug.cgi?id=33655
namespace detail {

template <class...>
struct make_void {
  using type = void;
};

}  // namespace detail

template <class... Ts>
using void_t = typename detail::make_void<Ts...>::type;

// Removes cvr qualifiers.
template <class T>
struct remove_cvref {
  typedef std::remove_cv_t<std::remove_reference_t<T>> type;
};

template <class T>
using remove_cvref_t = typename remove_cvref<T>::type;

// Same as `std::underlying_type_t` except that this one is SFINAE friendly.
template <class T, class = void>
struct underlying_type {
  using type = void;
};

template <class T>
struct underlying_type<T, std::enable_if_t<std::is_enum_v<T>>> {
  using type = std::underlying_type_t<T>;
};

template <class T>
using underlying_type_t = typename underlying_type<T>::type;

}  // namespace flare::internal

#endif  // FLARE_BASE_INTERNAL_META_H_
