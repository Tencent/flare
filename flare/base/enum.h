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

#ifndef FLARE_BASE_ENUM_H_
#define FLARE_BASE_ENUM_H_

#include <type_traits>

namespace flare {

// Specialize this trait and specify `value` to true for enabling bit-fields
// operators.
//
// `T` must be an enumeration type.
template <class T>
struct is_enum_bitmask_enabled {
  static constexpr bool value = false;
};

}  // namespace flare

namespace flare {

// But why don't you simply use `std::underlying_type_t<T>(v)`, instead of using
// this one?
template <class T, class = std::enable_if_t<std::is_enum_v<T>>>
constexpr auto underlying_value(T v) {
  return static_cast<std::underlying_type_t<T>>(v);
}

}  // namespace flare

// Intentionally put into global namespace.

template <class T,
          class = std::enable_if_t<flare::is_enum_bitmask_enabled<T>::value>>
constexpr T operator|(T left, T right) {
  return static_cast<T>(flare::underlying_value(left) |
                        flare::underlying_value(right));
}

template <class T,
          class = std::enable_if_t<flare::is_enum_bitmask_enabled<T>::value>>
constexpr T& operator|=(T& left, T right) {
  left = left | right;
  return left;
}

template <class T,
          class = std::enable_if_t<flare::is_enum_bitmask_enabled<T>::value>>
constexpr T operator&(T left, T right) {
  return static_cast<T>(flare::underlying_value(left) &
                        flare::underlying_value(right));
}

template <class T,
          class = std::enable_if_t<flare::is_enum_bitmask_enabled<T>::value>>
constexpr T& operator&=(T& left, T right) {
  left = left & right;
  return left;
}

template <class T,
          class = std::enable_if_t<flare::is_enum_bitmask_enabled<T>::value>>
constexpr T operator^(T left, T right) {
  return static_cast<T>(flare::underlying_value(left) ^
                        flare::underlying_value(right));
}

template <class T,
          class = std::enable_if_t<flare::is_enum_bitmask_enabled<T>::value>>
constexpr T& operator^=(T& left, T right) {
  left = left ^ right;
  return left;
}

template <class T,
          class = std::enable_if_t<flare::is_enum_bitmask_enabled<T>::value>>
constexpr T operator~(T value) {
  return static_cast<T>(~flare::underlying_value(value));
}

template <class T,
          class = std::enable_if_t<flare::is_enum_bitmask_enabled<T>::value>>
constexpr bool operator!(T value) {
  return !flare::underlying_value(value);
}

#endif  // FLARE_BASE_ENUM_H_
