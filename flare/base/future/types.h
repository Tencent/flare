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
// This file defines type `Types`, not a collection of types..
//
// I'm not sure if I should reinvent the wheel, there's already an
// implementation in boost.mpl.

#ifndef FLARE_BASE_FUTURE_TYPES_H_
#define FLARE_BASE_FUTURE_TYPES_H_

#include <type_traits>

namespace flare::future {

// Helper type for playing with type-system.
template <class... Ts>
struct Types {};

// Get type at the specified location.
template <class T, std::size_t I>
struct types_at;
template <class T, class... Ts>
struct types_at<Types<T, Ts...>, 0> {
  using type = T;
};  // Recursion boundary.
template <std::size_t I, class T, class... Ts>
struct types_at<Types<T, Ts...>, I> : types_at<Types<Ts...>, I - 1> {};

template <class T, std::size_t I>
using types_at_t = typename types_at<T, I>::type;
static_assert(std::is_same_v<types_at_t<Types<int, char, void>, 1>, char>);

// Analogous to `std::tuple_cat`.
template <class... Ts>
struct types_cat;
template <>
struct types_cat<> {
  using type = Types<>;
};  // Special case.
template <class... Ts>
struct types_cat<Types<Ts...>> {
  using type = Types<Ts...>;
};  // Recursion boundary.
template <class... Ts, class... Us, class... Vs>
struct types_cat<Types<Ts...>, Types<Us...>, Vs...>
    : types_cat<Types<Ts..., Us...>, Vs...> {};

template <class... Ts>
using types_cat_t = typename types_cat<Ts...>::type;
static_assert(std::is_same_v<types_cat_t<Types<int, double>, Types<void*>>,
                             Types<int, double, void*>>);
static_assert(
    std::is_same_v<types_cat_t<Types<int, double>, Types<void*>, Types<>>,
                   Types<int, double, void*>>);
static_assert(std::is_same_v<
              types_cat_t<Types<int, double>, Types<void*>, Types<unsigned>>,
              Types<int, double, void*, unsigned>>);

// Check to see if a given type is listed in the types.
template <class T, class U>
struct types_contains;
template <class U>
struct types_contains<Types<>, U> : std::false_type {};
template <class T, class U>
struct types_contains<Types<T>, U> : std::is_same<T, U> {};
template <class T, class U, class... Ts>
struct types_contains<Types<T, Ts...>, U>
    : std::conditional_t<std::is_same_v<T, U>, std::true_type,
                         types_contains<Types<Ts...>, U>> {};

template <class T, class U>
constexpr auto types_contains_v = types_contains<T, U>::value;
static_assert(types_contains_v<Types<int, char>, char>);
static_assert(!types_contains_v<Types<int, char>, char*>);

// Erase all occurrance of a given type from `Types<...>`.
template <class T, class U>
struct types_erase;
template <class U>
struct types_erase<Types<>, U> {
  using type = Types<>;
};  // Recursion boundary.
template <class T, class U, class... Ts>
struct types_erase<Types<T, Ts...>, U>
    : types_cat<std::conditional_t<!std::is_same_v<T, U>, Types<T>, Types<>>,
                typename types_erase<Types<Ts...>, U>::type> {};

template <class T, class U>
using types_erase_t = typename types_erase<T, U>::type;
static_assert(std::is_same_v<types_erase_t<Types<int, void, char>, void>,
                             Types<int, char>>);
static_assert(std::is_same_v<types_erase_t<Types<>, void>, Types<>>);
static_assert(
    std::is_same_v<types_erase_t<Types<int, char>, void>, Types<int, char>>);

}  // namespace flare::future

#endif  // FLARE_BASE_FUTURE_TYPES_H_
