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

#ifndef FLARE_BASE_FUTURE_BASICS_H_
#define FLARE_BASE_FUTURE_BASICS_H_

#include <tuple>
#include <type_traits>

#include "flare/base/future/types.h"

namespace flare::future {

// The helpers for easier use..
template <class... Ts>
constexpr auto are_rvalue_refs_v =
    std::conjunction_v<std::is_rvalue_reference<Ts>...>;
static_assert(are_rvalue_refs_v<int&&, char&&>);
static_assert(!are_rvalue_refs_v<int, char&&>);

// Rebinds `Ts...` in `TT<...>` to `UT<...>`.
//
// rebind_t<Future<int, double>, Promise<>> -> Promise<int, double>.
template <class T, template <class...> class UT>
struct rebind;
template <template <class...> class TT, class... Ts,
          template <class...> class UT>
struct rebind<TT<Ts...>, UT> {
  using type = UT<Ts...>;
};

template <class T, template <class...> class UT>
using rebind_t = typename rebind<T, UT>::type;
static_assert(std::is_same_v<rebind_t<std::common_type<>, Types>, Types<>>);
static_assert(std::is_same_v<rebind_t<std::common_type<int, char>, Types>,
                             Types<int, char>>);

// Basic templates.
template <class... Ts>
class Future;
template <class... Ts>
class Promise;
template <class... Ts>
class Boxed;

// Test if a given type `T` is a `Future<...>`.
template <class T>
struct is_future : std::false_type {};
template <class... Ts>
struct is_future<Future<Ts...>> : std::true_type {};

template <class T>
constexpr auto is_future_v = is_future<T>::value;
static_assert(!is_future_v<char*>);
static_assert(is_future_v<Future<>>);
static_assert(is_future_v<Future<char*>>);
static_assert(is_future_v<Future<char*, int>>);

template <class... Ts>
constexpr auto is_futures_v = std::conjunction_v<is_future<Ts>...>;

// "Futurize" a type.
template <class... Ts>
struct futurize {
  using type = Future<Ts...>;
};
template <>
struct futurize<void> : futurize<> {};
template <class... Ts>
struct futurize<Future<Ts...>> : futurize<Ts...> {};

template <class... Ts>
using futurize_t = typename futurize<Ts...>::type;
static_assert(std::is_same_v<futurize_t<int>, Future<int>>);
static_assert(std::is_same_v<futurize_t<Future<>>, Future<>>);
static_assert(std::is_same_v<futurize_t<Future<int>>, Future<int>>);

// Concatenate `Ts...` in multiple `Future<Ts...>`s.
//
// flatten<Future<T1, T2>, Future<T3>> -> Future<T1, T2, T3>.
template <class... Ts>
struct flatten {
  using type = rebind_t<types_cat_t<rebind_t<Ts, Types>...>, Future>;
};

template <class... Ts>
using flatten_t = typename flatten<Ts...>::type;
static_assert(
    std::is_same_v<flatten_t<Future<void*>, Future<>, Future<char>>,
                   Future<void*, char>>);  // Empty parameter pack is ignored.
static_assert(std::is_same_v<flatten_t<Future<>, Future<>>, Future<>>);

// Shortcuts for `rebind_t<..., Boxed>`, `rebind_t<..., Promise>`.
//
// Used primarily for rebinding `Future<Ts...>`.
template <class T>
using as_boxed_t = rebind_t<T, Boxed>;
template <class T>
using as_promise_t = rebind_t<T, Promise>;
static_assert(std::is_same_v<as_boxed_t<Future<>>, Boxed<>>);
static_assert(std::is_same_v<as_boxed_t<Future<int, char>>, Boxed<int, char>>);
static_assert(std::is_same_v<as_promise_t<Future<>>, Promise<>>);
static_assert(
    std::is_same_v<as_promise_t<Future<int, char>>, Promise<int, char>>);

// Value type we'll get from `Boxed<...>::Get`. Also used in several other
// places.
//
// Depending on what's in `Ts...`, `unboxed_type_t<Ts...>` can be:
//
// - `void` if `Ts...` is empty;
// - `T` is there's only one type `T` in `Ts...`.;
// - `std::tuple<Ts...>` otherwise.
template <class... Ts>
struct unboxed_type {
  using type = typename std::conditional_t<
      sizeof...(Ts) == 0, std::common_type<void>,
      std::conditional_t<sizeof...(Ts) == 1, std::common_type<Ts...>,
                         std::common_type<std::tuple<Ts...>>>>::type;
};

template <class... Ts>
using unboxed_type_t = typename unboxed_type<Ts...>::type;

}  // namespace flare::future

#endif  // FLARE_BASE_FUTURE_BASICS_H_
