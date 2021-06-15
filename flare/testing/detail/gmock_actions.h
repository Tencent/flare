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

#ifndef FLARE_TESTING_DETAIL_GMOCK_ACTIONS_H_
#define FLARE_TESTING_DETAIL_GMOCK_ACTIONS_H_

#include <tuple>
#include <utility>

#include "gmock/gmock.h"

#include "flare/base/internal/meta.h"

// IN MOST CASES YOU SHOULDN'T BE LOOKING HERE. CHECK OUT `xxx_mock.h` YOU'RE
// USING FOR WHAT YOU CAN USE WITH `flare::testing::Return`.

namespace flare::testing {

// Specialize this traits to map your implementation to mock interface.
template <class T>
struct MockImplementationTraits;

namespace detail {

// Implementation of `flare::testing::Return`.
template <class... Ts>
class ReturnImpl {
  using ValueTuple = std::tuple<Ts...>;

 public:
  explicit ReturnImpl(Ts... value) : values_(std::tuple(std::move(value)...)) {}

  template <typename F>
  operator ::testing::Action<F>() const {
    return ::testing::Action<F>(new Impl<F>(values_));
  }

 private:
  template <class F>
  class Impl : public ::testing::ActionInterface<F> {
    using Result = typename ::testing::ActionInterface<F>::Result;

#ifdef GTEST_RVALUE_REF_  // Internally we use a hacked version of GMock.
    using ArgumentTupleRef = GTEST_RVALUE_REF_(
        typename ::testing::ActionInterface<F>::ArgumentTuple);
#else
    using ArgumentTupleRef =
        const typename ::testing::ActionInterface<F>::ArgumentTuple&;
#endif

   public:
    explicit Impl(ValueTuple values) : values_(std::move(values)) {}

    Result Perform(ArgumentTupleRef arg_tuple) override {
      using ForwardTo = typename MockImplementationTraits<
          internal::remove_cvref_t<decltype(*std::get<0>(arg_tuple))>>::type;
      constexpr auto kArguments =
          std::tuple_size_v<std::remove_reference_t<decltype(arg_tuple)>>;
      // GCC 8.2 BUG (ICE) here: `std::make_index_sequence<kArguments - 1>{}`
      // can't be constructed in `apply_cb`.
      constexpr auto kIndices = std::make_index_sequence<kArguments - 1>{};

      auto apply_cb = [&](auto&&... args) {
        return ForwardTo::GMockActionReturn(MoveArgs<1>(arg_tuple, kIndices),
                                            args...);
      };
      return std::apply(apply_cb, values_);
    }

   private:
    template <std::size_t From, std::size_t... Indices, class U>
    static auto MoveArgs(U&& from,
                         std::integer_sequence<std::size_t, Indices...>) {
      return std::forward_as_tuple(
          std::get<From + Indices>(std::forward<U>(from))...);
    }

   private:
    ValueTuple values_;
  };

 private:
  ValueTuple values_;
};

}  // namespace detail

// Forwards call to `decltype(arg0)::GMockActionReturn` with (args_tuple,
// values_captured_on_construction...).
//
// TODO(luobogao): I'd prefer `Return(1, {"a", "b"})` to construct an object of
// type `ReturnImpl<int, std::vector<std::string>>` (currently it would be
// `ReturnImpl<int, std::initializer_list<const char*>>`). I think it should be
// possible (at least by introducing overloads), let's review this later.
template <class... Ts>
auto Return(Ts&&... args) {
  return detail::ReturnImpl(std::forward<Ts>(args)...);
}

}  // namespace flare::testing

#endif  // FLARE_TESTING_DETAIL_GMOCK_ACTIONS_H_
