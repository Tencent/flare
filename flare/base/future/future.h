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

#ifndef FLARE_BASE_FUTURE_FUTURE_H_
#define FLARE_BASE_FUTURE_FUTURE_H_

#include <memory>
#include <tuple>
#include <utility>

#include "flare/base/future/basics.h"
#include "flare/base/future/boxed.h"
#include "flare/base/future/core.h"

namespace flare::future {

// The names duplicate what `box_values_t` / ... serve for..
//
// We could have chosen `from_values_t` / ... to unify the names, but the
// corresponding variable `from_values` / ... is likely to clash with some
// methods' name..
struct futurize_values_t {
  explicit constexpr futurize_values_t() = default;
};  // @sa: LWG 2510
struct futurize_tuple_t {
  explicit constexpr futurize_tuple_t() = default;
};

inline constexpr futurize_values_t futurize_values;
inline constexpr futurize_tuple_t futurize_tuple;

// `Future` can be used to receive result from an asynchronous operation.
//
// If the `Future` is destroyed before the operation completes, the operation is
// detached (i.e., the result is discarded.).
template <class... Ts>
class Future {
 public:
  // Construct a "ready" future from immediate values.
  //
  // This overload does not participate in overload resolution unless
  // `std::tuple<Ts...>` (or `Boxed<Ts...>`, to be more specifically) is
  // constructible from `Us&&...`.
  template <class... Us, class = std::enable_if_t<std::is_constructible_v<
                             Boxed<Ts...>, box_values_t, Us&&...>>>
  explicit Future(futurize_values_t, Us&&... imms);

  // Construct a "ready" future from `std::tuple<Us...>` as long as
  // `Boxed<Ts...>` is constructible from `Boxed<Us...>`.
  //
  // We're accepting values (rather than refs) here, performance may hurt
  // if something like `std::array<int, 12345678>` is listed in `Ts...`.
  template <class... Us, class = std::enable_if_t<std::is_constructible_v<
                             Boxed<Ts...>, Boxed<Us...>>>>
  Future(futurize_tuple_t, std::tuple<Us...> values);

  // Construct a "ready" future from an immediate value. Shortcut for
  // `Future(futurize_values, value)`.
  //
  // Only participate in overload resolution when:
  //
  // - `sizeof...(Ts)` is 1;
  // - `T` (The only type in `Ts...`) is constructible from `U`.
  // - `U` is not `Future<...>`.
  template <class U, class = std::enable_if_t<
                         std::is_constructible_v<
                             typename std::conditional_t<
                                 sizeof...(Ts) == 1, std::common_type<Ts...>,
                                 std::common_type<void>>::type,
                             U> &&
                         !is_future_v<U>>>
  /* implicit */ Future(U&& value);

  // Conversion between compatible types is allowed.
  //
  // future: Its value (once satisfied) is used to satisfy the `Future` being
  //         constructed. Itself is invalidated on return.
  template <class... Us, class = std::enable_if_t<std::is_constructible_v<
                             Boxed<Ts...>, Boxed<Us...>>>>
  /* implicit */ Future(Future<Us...>&& future);  // Rvalue-ref intended.

  // Default constructor constructs an empty future which is not of much use
  // except for being a placeholder.
  //
  // It might be easier for our user to use if the default constructor instead
  // constructs a ready future if `Ts...` is empty, though. Currently they're
  // forced to write `Future(futurize_values)`.
  //
  //   If we do this, we can allow `Future(1, 2.0, "3")` to construct a ready
  //   future of type `Future<int, double, const char *>` as well. This really
  //   is a nice feature to have.
  //
  //   The reason we currently don't do this is that it's just inconsistent
  //   with the cases when `Ts...` is not empty, unless we also initialize a
  //   ready future in those cases. But if we do initialize the `Future` that
  //   way, the constructor can be too heavy to justify the convenience this
  //   brings. Besides, this will require `Ts...` to be DefaultConstructible,
  //   which currently we do not require.
  Future() = default;

  // Non-copyable.
  //
  // DECLARE_UNCOPYABLE / Inheriting from `Uncopyable` prevents the compiler
  // from generating move constructor / operator, leaving us to defaulting
  // those members.
  //
  // But using the macro for uncopyable while rolling our own for moveable
  // makes the code harder to reason about, so we explicitly delete the copy
  // constructor / operator here.
  Future(const Future&) = delete;
  Future& operator=(const Future&) = delete;
  // Movable.
  Future(Future&&) = default;
  Future& operator=(Future&&) = default;

  // It's meaningless to use `void` in `Ts...`, simply use `Future<>` is
  // enough.
  //
  // Let's raise a hard error for `void` then.
  static_assert(!types_contains_v<Types<Ts...>, void>,
                "There's no point in specifying `void` in `Ts...`, use "
                "`Future<>` if you meant to declare a future with no value.");

  // Given that:
  //
  // - We support chaining continuation;
  // - The value satisfied us is / will be *moved* into continuation's
  //   argument;
  //
  // It's fundamentally broken to provide a `Get` on `Future` as we have no
  // way to get the value back from the continuation's argument.
  //
  // `Get` and the continuation will race, as well.
  [[deprecated(
      "`Future` does not support blocking `Get`, use `BlockingGet` "
      "instead.")]] void
  Get();  // Not implemented.

  // `Then` chains a continuation to `Future`. The continuation is called
  // once the `Future` is satisfied.
  //
  // `Then<Executor, F>` might be implemented later if needed.
  //
  // continuation:
  //
  //   Either `continuation(Ts...)` or `continuation(Boxed<Ts...>)` should
  //   be well-formed. When both are valid, the former is preferred.
  //
  //     Should we allow continuations accepting no parameters (meaning that
  //     the user is going to ignore the value) be passed in even if `Ts...`
  //     is not empty? I tried to write `Future<Ts...>().Then([&] { set_flag();
  //     });` multiple times when writing UT, and don't see any confusion
  //     there. I'm not sure if it's of much use in practice though..
  //
  //   Please note that when the parameter types of `continuation` are declared
  //   with `auto` (or `auto &&`, or anything alike), it's technically
  //   infeasible to check whether the continuation is excepting `Ts...` or
  //   `Boxed<Ts...>`. In this case, the former is preferred.
  //
  //   If `Boxed<Ts...>` is needed, specify the type in `continuation`'s
  //   signature explicitly.
  //
  // Returns:
  //
  //   - In case the `continuation` returns another `Future`, a equivalent
  //     one is returned by `Then`;
  //   - Otherwise (the `continuation` returns a immediate value) `Then`
  //     returns a `Future` that is satisfied with the value `continuation`
  //     returns once it's called.
  //
  // Note:
  //
  //   `Then` is only allowed on rvalue-ref, use `std::move(future)` when
  //   needed.
  //
  //   Executor of result is inherited from *this. Even if the continuation
  //   returns a `Future` using a different executor, only the value (but
  //   not the executor) is forwarded to the result.
  template <class F,
            class R = futurize_t<typename std::conditional_t<
                std::is_invocable_v<F, Ts...>, std::invoke_result<F, Ts...>,
                std::invoke_result<F, Boxed<Ts...>>>::type>>
  R Then(F&& continuation) &&;

  // In most cases, `Then` is called on temporaries (which are rvalues)
  // rather than lvalues: `get_xxx_async().Then(...).Then(...)`, and this
  // overload won't be chosen.
  //
  // But in case it is chosen, we should prevent the code from compiling,
  // to force the user to mark the invalidation of the `Future` explicitly
  // using `std::move`.
  template <class F>
  [[deprecated(
      "`Future` must be rvalue-qualified to call `Then` on it. "
      "Use `std::move(future).Then(...)` instead.")]] void
  Then(F&&) &;  // Not implemented.

 private:
  friend class Promise<Ts...>;
  template <class... Us>
  friend class Future;

  // `Promise` uses this constructor to build `Future` in its `GetFuture`.
  explicit Future(std::shared_ptr<Core<Ts...>> core) : core_(std::move(core)) {}

  // Internal state shared with `Promise<...>`.
  std::shared_ptr<Core<Ts...>> core_;
};

// Deduction guides.

template <class... Us>
Future(futurize_values_t, Us&&...) -> Future<std::decay_t<Us>...>;
template <class... Us>
Future(futurize_tuple_t, std::tuple<Us...>) -> Future<std::decay_t<Us>...>;
template <class U>
Future(U&&) -> Future<std::decay_t<U>>;

// Implemented in a separate header.

}  // namespace flare::future

#endif  // FLARE_BASE_FUTURE_FUTURE_H_
