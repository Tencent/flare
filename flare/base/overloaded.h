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

#ifndef FLARE_BASE_OVERLOADED_H_
#define FLARE_BASE_OVERLOADED_H_

#include <type_traits>

namespace flare {

// This class helps you to create a functor that accepts different argument
// types, effectively create a "overloaded" version of lambda.
//
// Usage:
//
// std::visit(Overloaded{[](int) {},
//                       [](const std::string&) {},
//                       [](auto&&) { /* Catch all. */}},
//            some_variant);
//
// Note that you can only construct `Overloaded` with parentheses in C++20, in
// C++17 you need to use curly brackets to construct it.
template <class... Ts>
struct Overloaded : Ts... {
  using Ts::operator()...;
};

template <class... Ts>
Overloaded(Ts&&...) -> Overloaded<std::remove_reference_t<Ts>...>;

}  // namespace flare

#endif  // FLARE_BASE_OVERLOADED_H_
