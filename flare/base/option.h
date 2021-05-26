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

#ifndef FLARE_BASE_OPTION_H_
#define FLARE_BASE_OPTION_H_

#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>

#include "flare/base/option/option_impl.h"

namespace flare {

inline struct fixed_option_t {
  constexpr explicit fixed_option_t() = default;
} fixed_option;  // Option is NOT reloaded after initialization.
inline struct dynamic_option_t {
  constexpr explicit dynamic_option_t() = default;
} dynamic_option;  // Option is reloaded periodically.

// You can use `Option<T>` to import options from external configuration system
// (with the help of option provider (@sa: `option/option_provider.h`).
//
// For end users, you'd likely want to use `FeatureOption` /
// `RainbowOption<...>` instead. This "generic" template is only useful if you
// implemented your own option provider.
//
// We provided some operator overload / implicit conversion for easier you, so
// that you don't _always_ need to call `Option<T>::Get()`.
//
// Example:
//
// ```cpp
// DEFINE_int(crash_threshold, 10, "...");
//
// // For illustration purpose only. In real world you likely would like to
// stick
// // with other providers (@sa: `FeatureOption`, `RainbowOption<...>`).
// GflagsOptions<int> crash_opt("crash_threshold");
//
// void CrashingMethod() {
//   if (crash_opt > 10) {
//     std::terminate();  // More C++-ish than `std::abort()`. Hopefully you'd
//                        // always crash in this way. (if you _really_ want
//                        // your program to crash in the first place.)
//   }
// }
// ```
template <class T, class Parser = option::detail::IdentityParser<T>,
          class = void>
class Option : private option::OptionImpl<T, Parser> {
  // Exactly `T` for primitive types, otherwise it's `const T&`.
  using value_or_ref = typename option::OptionImpl<T, Parser>::value_or_ref;

 public:
  // Fixed options are only resolved once, at start-up time. If you cannot
  // handle option change well during execution, this overload is what you
  // should use.
  Option(fixed_option_t, const std::string& provider, option::MultiKey name,
         T default_value = T(), Function<bool(T)> validator = nullptr)
      : option::OptionImpl<T, Parser>(provider, std::move(name),
                                      std::move(default_value),
                                      std::move(validator), true) {}

  // For dynamic options, they can change at any time (calling `Get()`
  // concurrently is safe, don't worry), you must be prepared to handle value
  // change.
  Option(dynamic_option_t, const std::string& provider, option::MultiKey name,
         T default_value = T(), Function<bool(T)> validator = nullptr)
      : option::OptionImpl<T, Parser>(provider, std::move(name),
                                      std::move(default_value),
                                      std::move(validator), false) {}

  // If neither `fixed_option` nor `dynamic_option` is given to constructor, by
  // default a "dynamic" one is constructed.
  Option(const std::string& provider, option::MultiKey name,
         T default_value = T(), Function<bool(T)> validator = nullptr)
      : Option(dynamic_option, provider, std::move(name),
               std::move(default_value), std::move(validator)) {}

  using option::OptionImpl<T, Parser>::Get;
  using option::OptionImpl<T, Parser>::operator value_or_ref;
};

// Uses "GFlags" as option provider.
//
// YOU SHOULDN'T BE USING THIS ONE, IT PROVIDES ABSOLUTELY NO BENIFITS OVER
// USING GFLAGS DIRECTLY. THIS CLASS IS ONLY A PROOF-OF-CONCEPT (mainly for
// testing purpose, for you and for us.).
template <class T, class Parser = option::detail::IdentityParser<T>>
class GflagsOptions : public Option<T, Parser> {
 public:
  // Reading from GFlags should never fail, so no default value is needed.
  //
  // `GflagsOptions`s are always dynamic. If you don't want them to change, do
  // not allow GFlags to change.
  explicit GflagsOptions(option::Key name)
      : Option<T, Parser>(dynamic_option, "gflags",
                          option::MultiKey(std::move(name))) {}
};

namespace option {

// This is called automatically by `flare::Start`. You should not call it in
// most cases.
void InitializeOptions();

// This is usually called by `flare::Start`. You shouldn't call this method in
// most cases.
void ShutdownOptions();

// Immediately synchronizes options with its provider.
//
// Synchronization is done periodically by runtime, you only need to call this
// method if you need *immediate* synchronization.
void SynchronizeOptions();

// Dump all registered options, along with their values.
Json::Value DumpOptions();

}  // namespace option

//////////////////////////////////////////////
// Implementation goes below.               //
//////////////////////////////////////////////

// Operator overloaded for easier use.
//
// 3-way comparison should simplify the code.
#define FLARE_OPTION_DEFINE_OPERATOR_OVERLOAD_FOR(op)  \
  template <class... Ts, class U>                      \
  auto operator op(const Option<Ts...>& x, const U& y) \
      ->decltype(x.Get() op std::declval<U>()) {       \
    return x.Get() op y;                               \
  }                                                    \
  template <class... Ts, class U>                      \
  auto operator op(const U& x, const Option<Ts...>& y) \
      ->decltype(std::declval<U>() op y.Get()) {       \
    return x op y.Get();                               \
  }

FLARE_OPTION_DEFINE_OPERATOR_OVERLOAD_FOR(==)  // NOLINT
FLARE_OPTION_DEFINE_OPERATOR_OVERLOAD_FOR(!=)  // NOLINT
FLARE_OPTION_DEFINE_OPERATOR_OVERLOAD_FOR(<)   // NOLINT
FLARE_OPTION_DEFINE_OPERATOR_OVERLOAD_FOR(<=)  // NOLINT
FLARE_OPTION_DEFINE_OPERATOR_OVERLOAD_FOR(>)   // NOLINT
FLARE_OPTION_DEFINE_OPERATOR_OVERLOAD_FOR(>=)  // NOLINT
FLARE_OPTION_DEFINE_OPERATOR_OVERLOAD_FOR(&)   // NOLINT
FLARE_OPTION_DEFINE_OPERATOR_OVERLOAD_FOR(|)   // NOLINT
FLARE_OPTION_DEFINE_OPERATOR_OVERLOAD_FOR(&&)  // NOLINT
FLARE_OPTION_DEFINE_OPERATOR_OVERLOAD_FOR(||)  // NOLINT

#undef FLARE_OPTION_DEFINE_OPERATOR_OVERLOAD_FOR

// Support for iostream.
template <class... Ts>
std::ostream& operator<<(std::ostream& os, const Option<Ts...>& opt) {
  return os << opt.Get();
}

}  // namespace flare

#endif  // FLARE_BASE_OPTION_H_
