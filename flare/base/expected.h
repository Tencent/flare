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

#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

// @sa: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0323r6.html

namespace flare {

// TODO(?) Unexpected.

// A low quality mimic (as time of writting) of `std::expected<>` (P0323R6).
//
// TODO(?): Refine the interface using the definition in P0323R6.
template <class T, class E = void>
class Expected {
 public:
  constexpr Expected() = default;
  template <class U, class = std::enable_if_t<std::is_constructible_v<T, U> &&
                                              std::is_convertible_v<U&&, T>>>
  constexpr /* implicit */ Expected(U&& value)
      : value_(std::forward<U>(value)) {}
  constexpr /* implicit */ Expected(E error)
      : value_(std::in_place_index<1>, std::move(error)) {}
  constexpr T* operator->() { return &value(); }
  constexpr const T* operator->() const { return &value(); }
  constexpr T& operator*() { return value(); }
  constexpr const T& operator*() const { return value(); }
  constexpr explicit operator bool() const noexcept {
    return value_.index() == 0;
  }
  constexpr T& value() { return std::get<0>(value_); }
  constexpr const T& value() const { return std::get<0>(value_); }
  constexpr E& error() { return std::get<1>(value_); }
  constexpr const E& error() const { return std::get<1>(value_); }
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
  constexpr Expected() = default;
  constexpr /* implicit */ Expected(E error) : error_(std::move(error)) {}
  constexpr explicit operator bool() const noexcept { return !error_; }
  constexpr E& error() { return *error_; }
  constexpr const E& error() const { return *error_; }

 private:
  std::optional<E> error_;
};

// TODO(?): `operator==`, `operator!=`

}  // namespace flare

#endif  // FLARE_BASE_EXPECTED_H_
