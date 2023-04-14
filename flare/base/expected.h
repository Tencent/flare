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

#ifndef FLARE_BASE_EXPECTED_H_
#define FLARE_BASE_EXPECTED_H_

#include <initializer_list>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

#include "flare/base/internal/logging.h"
#include "flare/base/internal/meta.h"

// @sa: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0323r6.html

namespace flare {

template <class E>
class Unexpected {
 public:
  constexpr Unexpected(const Unexpected&) = default;
  constexpr Unexpected(Unexpected&&) = default;

  template <
      class Err = E,
      class = std::enable_if_t<
          !std::is_same_v<internal::remove_cvref_t<Err>, Unexpected> &&
          !std::is_same_v<internal::remove_cvref_t<Err>, std::in_place_t> &&
          std::is_constructible_v<E, Err>>>
  constexpr explicit Unexpected(Err&& e) noexcept(
      std::is_nothrow_constructible_v<E, Err>)
      : unex_(std::forward<Err>(e)) {}

  template <class... Args,
            class = std::enable_if_t<std::is_constructible_v<E, Args...>>>
  constexpr explicit Unexpected(std::in_place_t, Args&&... args) noexcept(
      std::is_nothrow_constructible_v<E, Args...>)
      : unex_(std::forward<Args>(args)...) {}

  template <class Up, class... Args,
            class = std::enable_if_t<std::is_constructible_v<
                E, std::initializer_list<Up>&, Args...>>>
  constexpr explicit Unexpected(
      std::in_place_t, std::initializer_list<Up>& il,
      Args&&... args) noexcept(std::
                                   is_nothrow_constructible_v<
                                       E, std::initializer_list<Up>&, Args...>)
      : unex_(il, std::forward<Args>(args)...) {}

  constexpr Unexpected& operator=(const Unexpected&) = default;
  constexpr Unexpected& operator=(Unexpected&&) = default;

  [[nodiscard]] constexpr const E& error() const& noexcept { return unex_; }
  [[nodiscard]] constexpr E& error() & noexcept { return unex_; }
  [[nodiscard]] constexpr const E&& error() const&& noexcept {
    return std::move(unex_);
  }
  [[nodiscard]] constexpr E&& error() && noexcept { return std::move(unex_); }
  constexpr void swap(Unexpected& other) noexcept(
      std::is_nothrow_swappable_v<E>) {
    using std::swap;
    swap(unex_, other.unex);
  }

  template <class Err>
  [[nodiscard]] friend constexpr bool operator==(const Unexpected& x,
                                                 const Unexpected<Err> y) {
    return x.unex_ == y.error();
  }

  friend constexpr void swap(Unexpected& x,
                             Unexpected& y) noexcept(noexcept(x.swap(y))) {
    x.swap(y);
  }

 private:
  E unex_;
};

template <typename E>
Unexpected(E) -> Unexpected<E>;

// A low quality mimic (as time of writting) of `std::expected<>` (P0323R6).
//
// TODO(?): Refine the interface using the definition in P0323R6.
template <class T, class E>
class Expected {
 public:
  using value_type = T;
  using error_type = E;
  using unexpected_type = Unexpected<E>;

  constexpr Expected() = default;
  template <class U, class = std::enable_if_t<std::is_constructible_v<T, U> &&
                                              std::is_convertible_v<U&&, T>>>
  constexpr /* implicit */ Expected(U&& value)
      : value_(std::forward<U>(value)) {}
  constexpr /* implicit */ Expected(E error)
      : value_(std::in_place_index<1>, std::move(error)) {}

  template <class G = E,
            std::enable_if_t<std::is_constructible_v<E, const G&>>* = nullptr,
            std::enable_if_t<!std::is_convertible_v<const G&, E>>* = nullptr>
  constexpr explicit Expected(const Unexpected<G>& u) noexcept(
      std::is_nothrow_constructible_v<E, const G&>)
      : value_(std::in_place_index<1>, u.error()) {}

  template <class G = E,
            std::enable_if_t<std::is_constructible_v<E, const G&>>* = nullptr,
            std::enable_if_t<std::is_convertible_v<const G&, E>>* = nullptr>
  constexpr Expected(const Unexpected<G>& u) noexcept(
      std::is_nothrow_constructible_v<E, const G&>)
      : value_(std::in_place_index<1>, u.error()) {}

  template <class G = E,
            std::enable_if_t<std::is_constructible_v<E, G>>* = nullptr,
            std::enable_if_t<!std::is_convertible_v<G, E>>* = nullptr>
  constexpr explicit Expected(Unexpected<G>&& u) noexcept(
      std::is_nothrow_constructible_v<E, G>)
      : value_(std::in_place_index<1>, std::move(u).error()) {}

  template <class G = E,
            std::enable_if_t<std::is_constructible_v<E, G>>* = nullptr,
            std::enable_if_t<std::is_convertible_v<G, E>>* = nullptr>
  constexpr Expected(Unexpected<G>&& u) noexcept(
      std::is_nothrow_constructible_v<E, G>)
      : value_(std::in_place_index<1>, std::move(u).error()) {}

  constexpr T* operator->() { return &value(); }
  constexpr const T* operator->() const { return &value(); }
  constexpr T& operator*() { return value(); }
  constexpr const T& operator*() const { return value(); }
  constexpr bool has_value() const noexcept { return value_.index() == 0; }
  constexpr explicit operator bool() const noexcept {
    return value_.index() == 0;
  }
  [[nodiscard]] constexpr T& value() & {
    FLARE_CHECK(has_value(), "Expected has no value");
    return std::get<0>(value_);
  }
  [[nodiscard]] constexpr const T& value() const& {
    FLARE_CHECK(has_value(), "Expected has no value");
    return std::get<0>(value_);
  }
  [[nodiscard]] constexpr T&& value() && {
    FLARE_CHECK(has_value(), "Expected has no value");
    return std::move(std::get<0>(value_));
  }
  [[nodiscard]] constexpr const T&& value() const&& {
    FLARE_CHECK(has_value(), "Expected has no value");
    return std::move(std::get<0>(value_));
  }

  [[nodiscard]] constexpr E& error() & {
    FLARE_CHECK(!has_value(), "Expected has no error");
    return std::get<1>(value_);
  }
  [[nodiscard]] constexpr const E& error() const& {
    FLARE_CHECK(!has_value(), "Expected has no error");
    return std::get<1>(value_);
  }

  [[nodiscard]] constexpr E&& error() && {
    FLARE_CHECK(!has_value(), "Expected has no error");
    return std::move(std::get<1>(value_));
  }

  [[nodiscard]] constexpr const E&& error() const&& {
    FLARE_CHECK(!has_value(), "Expected has no error");
    return std::move(std::get<1>(value_));
  }

  template <class U>
  constexpr T value_or(U&& alternative) const {
    if (*this) {
      return value();
    } else {
      return std::forward<U>(alternative);
    }
  }
  // TODO(?): Overload of `ValueOr` for rvalue-ref.
 private:
  std::variant<T, E> value_;
};

template <class E>
class Expected<void, E> {
 public:
  using value_type = void;
  using error_type = E;
  using unexpected_type = Unexpected<E>;

  constexpr Expected() = default;
  constexpr /* implicit */ Expected(E error) : error_(std::move(error)) {}

  template <class G = E,
            std::enable_if_t<std::is_constructible_v<E, const G&>>* = nullptr,
            std::enable_if_t<!std::is_convertible_v<const G&, E>>* = nullptr>
  constexpr explicit Expected(const Unexpected<G>& u) noexcept(
      std::is_nothrow_constructible_v<E, const G&>)
      : error_(u.error()) {}

  template <class G = E,
            std::enable_if_t<std::is_constructible_v<E, const G&>>* = nullptr,
            std::enable_if_t<std::is_convertible_v<const G&, E>>* = nullptr>
  constexpr Expected(const Unexpected<G>& u) noexcept(
      std::is_nothrow_constructible_v<E, const G&>)
      : error_(u.error()) {}

  template <class G = E,
            std::enable_if_t<std::is_constructible_v<E, G>>* = nullptr,
            std::enable_if_t<!std::is_convertible_v<G, E>>* = nullptr>
  constexpr explicit Expected(Unexpected<G>&& u) noexcept(
      std::is_nothrow_constructible_v<E, G>)
      : error_(std::move(u).error()) {}

  template <class G = E,
            std::enable_if_t<std::is_constructible_v<E, G>>* = nullptr,
            std::enable_if_t<std::is_convertible_v<G, E>>* = nullptr>
  constexpr Expected(Unexpected<G>&& u) noexcept(
      std::is_nothrow_constructible_v<E, G>)
      : error_(std::move(u).error()) {}

  template <class... Args,
            class = std::enable_if_t<std::is_constructible_v<E, Args...>>>
  constexpr explicit Expected(unexpect_t, Args&&... args) noexcept(
      std::is_nothrow_constructible_v<E, Args...>)
      : error_(std::forward<Args>(args)...) {}

  template <class Up, class... Args,
            class = std::enable_if_t<std::is_constructible_v<
                E, std::initializer_list<Up>&, Args...>>>
  constexpr explicit Expected(
      unexpect_t, std::initializer_list<Up> il,
      Args&&... args) noexcept(std::
                                   is_nothrow_constructible_v<
                                       E, std::initializer_list<Up>&, Args...>)
      : error_(il, std::forward<Args>(args)...) {}
  constexpr explicit operator bool() const noexcept { return !error_; }
  constexpr E& error() { return *error_; }
  constexpr const E& error() const { return *error_; }

 private:
  std::optional<E> error_;
};

// TODO(?): `operator==`, `operator!=`

}  // namespace flare

#endif  // FLARE_BASE_EXPECTED_H_
