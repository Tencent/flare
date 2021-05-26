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

#ifndef FLARE_BASE_FUTURE_UTILS_H_
#define FLARE_BASE_FUTURE_UTILS_H_

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

#include "flare/base/future/basics.h"
#include "flare/base/future/impls.h"

// For each utility method that accepts a (or a collection of) r-value ref
// to `Future`, we also provided an overload that accepts a pointer to
// `Future`.
//
// It's just too annoying to type `Utility(std::move(...))` all the time.
// By `Utility(&var)`, I think it's striking enough to notice `var` being
// mutated, and the saving on typing justifies the overload's presence.
//
// But also note that the original version that accepts a r-value ref is still
// needed, so that snippets like `BlockingGet(OpenAsync())` compile.

namespace flare::future::utils {

// Creates a "ready" future from values.
template <class... Ts>
Future<std::decay_t<Ts>...> MakeReadyFuture(Ts&&... values);

// Creates a (possibly "ready") `Future<>` by calling `future` with `args...`.
template <class F, class... Args,
          class R = futurize_t<std::invoke_result_t<F, Args...>>>
R MakeFutureWith(F&& functor, Args&&... args);

// As the `Future<...>` itself does not support blocking, we provide this
// helper for blocking on `Future<...>`.
//
// The `future` must NOT have continuation chained, otherwise the behavior
// is undefined.
//
// The `future` passed in is move away so it's no longer usable after
// calling this method.
//
// Calling this method may lead to deadlock if the `future` we're blocking
// on is scheduled to be running on the same thread later.
//
// future: `Future` to block on. Invalidated on return.
// Returns: Value with which the `Future` is satisfied with.
//
//          - If the `Ts...` is empty, the return type is `void`;
//          - If there's only one type in `Ts...`, that type is the return
//            type.
//          - Otherwise `std::tuple<Ts...>` is returned.
//
//          The value in `future` is *moved out* into the return value.
template <class... Ts>
auto BlockingGet(Future<Ts...>&& future) -> unboxed_type_t<Ts...>;

// Same as `BlockingGet` except that it allows specifying timeout, and returns
// a `std::optional` instead (in case `Ts...` is empty, a `bool` is returned
// instead.).
template <class... Ts, class DurationOrTimePoint>
auto BlockingTryGet(Future<Ts...>&& future, const DurationOrTimePoint& timeout)
    -> future::detail::optional_or_bool_t<unboxed_type_t<Ts...>>;

// Same as `BlockingGet`, except that this one returns a `Boxed<...>`.
template <class... Ts>
auto BlockingGetPreservingErrors(Future<Ts...>&& future) -> Boxed<Ts...>;

template <class... Ts, class DurationOrTimePoint>
auto BlockingTryGetPreservingErrors(Future<Ts...>&& future,
                                    const DurationOrTimePoint& timeout)
    -> std::optional<Boxed<Ts...>>;

// Returns a `Future` that is satisfied once all of the `Future`s passed
// in are satisfied.
//
// Note that even if `std::vector<bool>` is special in that it's not guaranteed
// to be thread safe to access different elements concurrently, calling
// `WhenAll` on `std::vector<Future<bool>>` (resulting in `std::vector<bool>`)
// won't lead to data race.
//
// futures: `Future`s to wait. All of them are invalidated on return.
// Returns: `Future` satisfied with all of the values that are used to satisfy
//          `futures`, with order preserved. The return value is constructed
//          as if `std::tuple(BlockingGet(futures)...)` (with all `void`s
//          removed) is used to construct a ready `Future`.
//
//          It confused me for some time if we should group the values in
//          return type or flatten them, i.e., given `WhenAll(Future<>(),
//          Future<int>(), Future<char, double>()>`, should the return type
//          be:
//
//          - Future<int, std::tuple<char, double>> (what we currently do), or
//          - Future<int, char, double>?
//
//          (I personally object to use `Future<std::tuple<>, std::tuple<int>,
//          std::tuple<char, double>>`, it's unnecessarily complicated.)
template <class... Ts,  // `Ts` can't be ref types.
          class = std::enable_if_t<(sizeof...(Ts) > 0) && is_futures_v<Ts...> &&
                                   are_rvalue_refs_v<Ts&&...>>,
          class R = rebind_t<
              types_erase_t<Types<decltype(BlockingGet(std::declval<Ts>()))...>,
                            void>,
              Future>>
auto WhenAll(Ts&&... futures) -> R;

// Returns a `Future` that is satisfied once all of the `Future`s passed
// in are satisfied.
//
// futures: Same as `WhenAll`.
// Returns: `Future` satisfied with all of the boxed values that are used to
//          satisfy `futures`, with order preserved. For each `Future<Ts...>`
//          in `futures`, `Boxed<Ts...>` will be placed in the corresponding
//          position in the return type.
//
//          e.g. `WhenAll(Future<>(), Future<int>(), Future<double, char>())`
//          will be type `Future<Boxed<>>, Boxed<int>, Boxed<double, char>>`.
template <class... Ts,
          class = std::enable_if_t<(sizeof...(Ts) > 0) && is_futures_v<Ts...> &&
                                   are_rvalue_refs_v<Ts&&...>>>
auto WhenAllPreservingErrors(Ts&&... futures) -> Future<as_boxed_t<Ts>...>;

// Overload for a collection of homogeneous objects in a container.
//
// `Future`s in `futures` are invalidated on return.
//
// Returns:
//   - `void` if `Ts...` is empty. (i.e., `C<Future<>>` is passed in.)
//   - A collection of values with the same type as is `BlockingGet(Future<
//     Ts...>())`, with order preserved.
template <
    template <class...> class C, class... Ts,
    class R = std::conditional_t<std::is_void_v<unboxed_type_t<Ts...>>,
                                 Future<>, Future<C<unboxed_type_t<Ts...>>>>>
auto WhenAll(C<Future<Ts...>>&& futures) -> R;

// Overload for a collection of homogeneous objects in a container.
//
// Returns: A collection of `Boxed<Ts...>`s, with order preserved.
template <template <class...> class C, class... Ts>
auto WhenAllPreservingErrors(C<Future<Ts...>>&& futures)
    -> Future<C<Boxed<Ts...>>>;

// Returns a `Future` that is satisfied when any of the `Future`s passed in
// is satisfied.
//
// futures: `Future`s to wait. All of them are invalidated on return.
//
//          It's UNDEFINED to call `WhenAny` on an empty collection.
//
//          This is an implementation limitation: If we do allow an empty
//          collection be passed in, we'd have to default construct `Ts...`
//          on return. I'm not sure if handling this corner case justify the
//          burden we put on the user by requiring `Ts...` to be
//          DefaultConstructible.
//
//          But even if it's implementable without requiring `Ts...` to be
//          DefaultConstructible, it's still hard to tell what does it mean
//          to "wait for an object from nothing".
//
//          This is an inconsistency between `WhenAll` and `WhenAny`.
//
// Returns: `Future` satisfied with both the index of the first satisfied
//          `Future` and its value (the type is the same type as returned
//          by `BlockingGet`). If there's no value in `futures` (i.e.,
//          `Ts...` is empty), only the index will be saved in the `Future`
//          returned.
template <template <class...> class C, class... Ts,
          class R = std::conditional_t<
              std::is_void_v<unboxed_type_t<Ts...>>, Future<std::size_t>,
              Future<std::size_t, unboxed_type_t<Ts...>>>>
auto WhenAny(C<Future<Ts...>>&& futures) -> R;

// Same as above except for return value type, which holds a Boxed<Ts...>
// instead of values.
//
// `futures` may NOT be empty. (See comments above `WhenAny`'s declaration.)
template <template <class...> class C, class... Ts>
auto WhenAnyPreservingErrors(C<Future<Ts...>>&& futures)
    -> Future<std::size_t, Boxed<Ts...>>;

// DEPRECATED: Use `Split(...)` instead.
//
// Counterintuitively, `Fork`ing a `Future` not only gives back a
// `Future` that is satisfied with the same value as the original
// one, but also mutates the `future` passed in, due to implementation
// limitations. Generally, the users of `Fork` need not to be aware of
// this.
//
// `Fork` requires `Ts...` to be CopyConstructible, naturally.
//
// future: pointer to `Future` to fork. Overwritten with an equivalent one
//         on return.
// Returns: Another `Future` that is satisfied with the same value
//          as of `f`.
// Usage:
//        Promise<int> p;
//        auto f = p.GetFuture();
//        auto f2 = Fork(&f);
//        p.SetValue(1);
//        // Now both `f` and `f2` are satisfied with `1`.
template <class... Ts>
Future<Ts...> Fork(Future<Ts...>* future);

// "Splitting" a `Future` into two. This can be handy if the result of a future
// is used in two code branches.
//
// `Ts...` need to be CopyConstructible, as obvious.
//
// Returns: Two `Future`s, which are satisfied once `future` is satisfied.
//          It's unspecified whether `Ts`... is moved or copied into the
//          resulting future.
//
// Usage:
//
//   Promise<int> p;
//   auto f = p.GetFuture();
//   auto&& [f1, f2] = Split(&f);
//   p.SetValue(1);
//   // Both `f1` and `f2` are satisfied with `1` now.
template <class... Ts>
std::pair<Future<Ts...>, Future<Ts...>> Split(Future<Ts...>&& future);
template <class... Ts>
std::pair<Future<Ts...>, Future<Ts...>> Split(Future<Ts...>* future);

// Keep calling `action` passed in until it returns false.
//
// action: Action to call. The action should accept no arguments and return
//         a type that is implicitly convertible to `bool` / `Future<bool>` (
//         i.e. the signature of `action` should be `bool ()` /
//         `Future<bool> ()`.). The loop stops when the value returned by
//         `action` is `false`.
template <class F,
          class = std::enable_if_t<std::is_invocable_r_v<bool, F> ||
                                   std::is_invocable_r_v<Future<bool>, F>>>
Future<> Repeat(F&& action);

// Keep calling `action` until `pred` returns false.
//
// action: Action to call. The action should accept no arguments, and its
//         return value is passed to `pred` to evaluate if the loop should
//         proceed.
//
// pred: Predicates if the loop should proceed. It accepts what's returned
//       by `action`, which could be zero or more values, and returns a `bool`
//       (or `Future<bool>`, as `Repeat` expects) to indicate if the loop
//       should continue.
//
//       In case `action` returns a `Future<...>`, number of arguments to
//       `pred` could by more than 1 (`action` returns `Future<int, double>`,
//       e.g.).
//
//       `pred` should not accept argument by value unless it's expecting a
//       cheap-to-copy type (`int`, for example).
//
// Returns: The value return by `action` in the last iteration is returned.
//
// CAUTION: Due to implementation limitation, looping without useing a non--
//          inline executor may lead to stack overflow.
template <
    class F, class Pred,
    class R = futurize_t<std::invoke_result_t<F>>,  // Implies `F` invocable.
    class = std::enable_if_t<std::is_convertible_v<
        decltype(std::declval<R>().Then(std::declval<Pred>())),  // `Pred`
                                                                 // accepts `R`.
        Future<bool>>>  // And the result of `Pred` is convertible to `bool`.
    >
R RepeatIf(F&& action, Pred&& pred);

// RepeatForever is not implemented (yet) as it seems of little use.

// Should we provide a `For(InitVal, Body, Pred)`? `Body` accepts `InitVal`
// or return value from the last round. Not sure if it's useful.

}  // namespace flare::future::utils

namespace flare::future::utils {  // Implementation goes below.

template <class... Ts>
Future<std::decay_t<Ts>...> MakeReadyFuture(Ts&&... values) {
  return Future(futurize_values, std::forward<Ts>(values)...);
}

template <class F, class... Args, class R>
R MakeFutureWith(F&& functor, Args&&... args) {
  if constexpr (std::is_void_v<std::invoke_result_t<F, Args...>>) {
    static_assert(std::is_same_v<R, Future<>>);

    functor(std::forward<Args>(args)...);
    return R(futurize_values);
  } else {
    return R(functor(std::forward<Args>(args)...));
  }
}

template <class... Ts>
auto BlockingGet(Future<Ts...>&& future) -> unboxed_type_t<Ts...> {
  return BlockingGetPreservingErrors(std::move(future)).Get();
}

template <class... Ts>
auto BlockingGetPreservingErrors(Future<Ts...>&& future) -> Boxed<Ts...> {
  std::condition_variable cv;
  std::mutex lock;
  std::optional<Boxed<Ts...>> receiver;

  // Once the `future` is satisfied, our continuation will move the
  // result into `receiver` and notify `cv` to wake us up.
  std::move(future).Then([&](Boxed<Ts...> boxed) noexcept {
    std::lock_guard<std::mutex> lk(lock);
    receiver.emplace(std::move(boxed));
    cv.notify_one();
  });

  // Block until our continuation wakes us up.
  std::unique_lock<std::mutex> lk(lock);
  cv.wait(lk, [&] { return !!receiver; });

  return std::move(*receiver);
}

template <class... Ts, class DurationOrTimePoint>
auto BlockingTryGet(Future<Ts...>&& future, const DurationOrTimePoint& timeout)
    -> future::detail::optional_or_bool_t<unboxed_type_t<Ts...>> {
  auto rc = BlockingTryGetPreservingErrors(std::move(future), timeout);
  if constexpr (sizeof...(Ts) == 0) {
    return !!rc;
  } else {
    if (rc) {
      return std::move(rc->Get());
    }
    return std::nullopt;
  }
}

template <class... Ts, class DurationOrTimePoint>
auto BlockingTryGetPreservingErrors(Future<Ts...>&& future,
                                    const DurationOrTimePoint& timeout)
    -> std::optional<Boxed<Ts...>> {
  struct State {
    std::condition_variable cv;
    std::mutex lock;
    std::optional<Boxed<Ts...>> receiver;
  };
  auto state = std::make_shared<State>();

  // `state` must be copied here, in case of timeout, we'll leave the scope
  // before continuation is fired.
  std::move(future).Then([state](Boxed<Ts...> boxed) noexcept {
    std::lock_guard<std::mutex> lk(state->lock);
    state->receiver.emplace(std::move(boxed));
    state->cv.notify_one();
  });

  // Block until our continuation wakes us up.
  std::unique_lock<std::mutex> lk(state->lock);
  if constexpr (future::detail::is_duration_v<DurationOrTimePoint>) {
    state->cv.wait_for(lk, timeout, [&] { return !!state->receiver; });
  } else {
    state->cv.wait_until(lk, timeout, [&] { return !!state->receiver; });
  }

  return std::move(state->receiver);  // Safe. we're holding the lock.
}

namespace detail {

struct Flatten {
  // This helper adds another `std::tuple` on top of `t` if there's at
  // least 2 elements in `t`.
  //
  // We'll use this below when we `std::tuple_cat`ing the tuples we get
  // from `boxes.GetRaw()`.
  //
  // Because `std::tuple_cat` will remove the outer `std::tuple` from
  // `Boxed<...>::GetRaw()`, simply concatenating the tuples will flatten
  // the values even for the tuples with more than 1 elements.
  template <class T>
  auto Rebox(T&& t) const {
    using Reboxed =
        std::conditional_t<(std::tuple_size_v<T> >= 2), std::tuple<T&&>,
                           T>;  // Forward by refs if we do rebox the value.
    return Reboxed(std::forward<T>(t));
  }

  // Here we:
  //
  // - Iterate through each `Boxed<...>`;
  // - Get the `std::tuple<...>` inside and `Rebox` it;
  // - Concatenate the tuples we got..
  //
  // No copy should occur, everything is moved.
  template <class T, std::size_t... Is>
  auto operator()(T&& t, std::index_sequence<Is...>) const {
    return std::tuple_cat(Rebox(std::move(std::get<Is>(t).GetRaw()))...);
  }
};

}  // namespace detail

template <class... Ts, class, class R>
auto WhenAll(Ts&&... futures) -> R {
  return WhenAllPreservingErrors(std::move(futures)...)
      .Then([](Boxed<as_boxed_t<Ts>...> boxes) -> R {
        // Here we get `std::tuple<Boxed<...>, Boxed<...>, Boxed<...>>`.
        auto&& raw = boxes.GetRaw();

        auto&& flattened =
            detail::Flatten()(raw, std::index_sequence_for<Ts...>());
        // `raw` was moved and invalidated.

        return Future(futurize_tuple, std::move(flattened));
      });
}

template <template <class...> class C, class... Ts, class R>
auto WhenAll(C<Future<Ts...>>&& futures) -> R {
  return WhenAllPreservingErrors(std::move(futures))
      .Then([](C<Boxed<Ts...>> boxed_values) {
        if constexpr (std::is_void_v<unboxed_type_t<Ts...>>) {
          // sizeof...(Ts) == 0.
          //
          // Nothing to return.
          (void)boxed_values;
          return;
        } else {
          C<unboxed_type_t<Ts...>> result;

          result.reserve(boxed_values.size());
          for (auto&& e : boxed_values) {
            result.emplace_back(std::move(e).Get());
          }
          return result;
        }
      });
}

template <class... Ts, class>
auto WhenAllPreservingErrors(Ts&&... futures) -> Future<as_boxed_t<Ts>...> {
  static_assert(sizeof...(Ts) != 0,
                "There's no point in waiting on an empty future pack..");

  struct Context {
    Promise<as_boxed_t<Ts>...> promise;
    std::tuple<as_boxed_t<Ts>...> receivers{
        future::detail::RetrieveBoxed<as_boxed_t<Ts>>()...};
    std::atomic<std::size_t> left{sizeof...(Ts)};
  };
  auto context = std::make_shared<Context>();

  for_each_indexed(
      [&](auto&& future, auto index) {
        // We chain a continuation for each future. The continuation will
        // satisfy the promise we made once all of the futures are satisfied.
        std::move(future).Then(
            [context](
                as_boxed_t<std::remove_reference_t<decltype(future)>> boxed) {
              // Save the result.
              std::get<decltype(index)::value>(context->receivers) =
                  std::move(boxed);
              // Satisfy the promise if we're the last one.
              if (!--context->left) {
                context->promise.SetValue(std::move(context->receivers));
              }
            });
      },
      futures...);

  return context->promise.GetFuture();
}

template <template <class...> class C, class... Ts>
auto WhenAllPreservingErrors(C<Future<Ts...>>&& futures)
    -> Future<C<Boxed<Ts...>>> {
  if (futures.empty()) {
    return Future(futurize_values, C<Boxed<Ts...>>());
  }

  struct Context {
    Promise<C<Boxed<Ts...>>> promise;
    C<Boxed<Ts...>> values;
    std::atomic<std::size_t> left;
  };
  auto context = std::make_shared<Context>();

  // We cannot inline the initialization in `Context` as `futures.size()`
  // is not constant.
  context->values.reserve(futures.size());
  context->left = futures.size();
  for (std::size_t index = 0; index != futures.size(); ++index) {
    context->values.emplace_back(future::detail::RetrieveBoxed<Boxed<Ts...>>());
  }

  for (std::size_t index = 0; index != futures.size(); ++index) {
    std::move(futures[index])
        .Then([index, context](Boxed<Ts...> boxed) mutable {
          context->values[index] = std::move(boxed);

          if (!--context->left) {
            context->promise.SetValue(std::move(context->values));
          }
        });
  }

  return context->promise.GetFuture();
}

template <template <class...> class C, class... Ts, class R>
auto WhenAny(C<Future<Ts...>>&& futures) -> R {
  return WhenAnyPreservingErrors(std::move(futures))
      .Then([](std::size_t index, Boxed<Ts...> values) {
        if constexpr (sizeof...(Ts) == 0) {
          (void)values;
          return index;
        } else {
          return Future(futurize_values, index, values.Get());
        }
      });
}

template <template <class...> class C, class... Ts>
auto WhenAnyPreservingErrors(C<Future<Ts...>>&& futures)
    -> Future<std::size_t, Boxed<Ts...>> {
  // I do want to return a ready future on empty `futures`, but this
  // additionally requires `Ts...` to be DefaultConstructible, which
  // IMO is an overkill.
  CHECK(!futures.empty()) << "Calling `WhenAny(PreservingErrors)` on an empty "
                             "collection is undefined. We simply couldn't "
                             "define what does 'wait for a single object in an "
                             "empty collection' mean.";
  struct Context {
    Promise<std::size_t, Boxed<Ts...>> promise;
    std::atomic<bool> ever_satisfied{};
  };
  auto context = std::make_shared<Context>();

  for (std::size_t index = 0; index != futures.size(); ++index) {
    std::move(futures[index]).Then([index, context](Boxed<Ts...> boxed) {
      if (!context->ever_satisfied.exchange(true)) {
        // We're the first `Future` satisfied.
        context->promise.SetValue(index, std::move(boxed));
      }
      // Nothing otherwise.
    });
  }

  return context->promise.GetFuture();
}

template <class... Ts>
Future<Ts...> Fork(Future<Ts...>* future) {
  Promise<Ts...> p;  // FIXME: The default executor (instead of `future`'s)
                     //        is used here.
  auto f = p.GetFuture();

  *future =
      std::move(*future).Then([p = std::move(p)](Boxed<Ts...> boxed) mutable {
        p.SetBoxed(boxed);  // Requires `Boxed<Ts...>` to be CopyConstructible.
        if constexpr (sizeof...(Ts) != 0) {
          return std::move(boxed.Get());
        } else {
          // Nothing to do for `void`.
        }
      });

  return f;
}

template <class... Ts>
std::pair<Future<Ts...>, Future<Ts...>> Split(Future<Ts...>&& future) {
  return Split(&future);
}

template <class... Ts>
std::pair<Future<Ts...>, Future<Ts...>> Split(Future<Ts...>* future) {
  auto rf = Fork(future);
  return std::pair(std::move(rf), std::move(*future));
}

template <class F, class>
Future<> Repeat(F&& action) {
  return RepeatIf(std::forward<F>(action), [](bool f) { return f; })
      .Then([](auto&&) {});
}

namespace detail {

template <class R, class APtr, class PPtr>
R RepeatIfImpl(APtr act, PPtr pred) {
  // Note that here we should not introduce races as we'll NOT call `action`
  // concurrently, i.e., we're always in a "single threaded" environment --
  // The second iteration won't start until the first one is finished.

  // Optimization potentials: `MakeFutureWith` is not strictly needed. We may
  // eliminate the extra `Then`s when `act` / `pred` returns a immediately
  // value (instead of `Future<...>`).
  //
  //   But keep an eye not to starve other jobs waiting for execution in our
  //   thread (once we use an executor).
  auto value = MakeFutureWith(*act);

  return std::move(value).Then([act = std::move(act),
                                pred = std::move(pred)](auto&&... v) mutable {
    auto keep_going = MakeFutureWith(*pred, v...);  // `v...` is not moved away.

    return std::move(keep_going)
        .Then([act = std::move(act), pred = std::move(pred),
               vp = std::make_tuple(std::move(v)...)](bool k) mutable {
          if (k) {
            return RepeatIfImpl<R>(std::move(act),
                                   std::move(pred));  // Recursion.
          } else {
            return R(futurize_tuple, std::move(vp));
          }
        });
  });
}

}  // namespace detail

template <class F, class Pred, class R, class>
R RepeatIf(F&& action, Pred&& pred) {
  // We'll need `action` and `pred` every iteration.
  //
  // Instead of moving it all over around (as Folly do), we move it into
  // heap once and keep a reference to it until the iteration is done.
  //
  // This way we should be able to save a lot of move (which is cheap but
  // nonetheless not free.).
  //
  // Indeed we still have to move the references (`std::shared_ptr`) around
  // (inside `RepeatIfImpl`) but that's much cheaper generally.
  //
  // Whether this is beneficial depends on how many times we loop, indeed.
  return detail::RepeatIfImpl<R>(
      std::make_shared<std::decay_t<F>>(std::forward<F>(action)),
      std::make_shared<std::decay_t<Pred>>(std::forward<Pred>(pred)));
}

template <class... Ts>
auto BlockingGet(Future<Ts...>* future) {
  return BlockingGet(std::move(*future));
}

template <class... Ts>
auto BlockingGetPreservingErrors(Future<Ts...>* future) {
  return BlockingGetPreservingErrors(std::move(*future));
}

template <class... Ts, class DurationOrTimePoint>
auto BlockingTryGet(Future<Ts...>* future, const DurationOrTimePoint& tp) {
  return BlockingTryGet(std::move(*future), tp);
}

template <class... Ts, class DurationOrTimePoint>
auto BlockingTryGetPreservingErrors(Future<Ts...>* future,
                                    const DurationOrTimePoint& tp) {
  return BlockingGetPreservingErrors(std::move(*future), tp);
}

template <class... Ts, class = std::enable_if_t<is_futures_v<Ts...>>>
auto WhenAll(Ts*... futures) {
  return WhenAll(std::move(*futures)...);
}

template <class... Ts, class = std::enable_if_t<is_futures_v<Ts...>>>
auto WhenAllPreservingErrors(Ts*... futures) {
  return WhenAllPreservingErrors(std::move(*futures)...);
}

template <template <class...> class C, class... Ts>
auto WhenAll(C<Future<Ts...>>* futures) {
  return WhenAll(std::move(*futures));
}

template <template <class...> class C, class... Ts>
auto WhenAllPreservingErrors(C<Future<Ts...>>* futures) {
  return WhenAllPreservingErrors(std::move(*futures));
}

template <template <class...> class C, class... Ts>
auto WhenAny(C<Future<Ts...>>* futures) {
  return WhenAny(std::move(*futures));
}

template <template <class...> class C, class... Ts>
auto WhenAnyPreservingErrors(C<Future<Ts...>>* futures) {
  return WhenAnyPreservingErrors(std::move(*futures));
}

}  // namespace flare::future::utils

namespace flare::future {

// To avoid ADL kicking in:
//
// > The set of declarations found by ordinary unqualified lookup and the set of
// > declarations found in all elements of the associated set produced by ADL,
// > are merged, with the following special rules
//
// >  1) using-directives in the associated namespaces are ignored
using namespace utils;

}  // namespace flare::future

#endif  // FLARE_BASE_FUTURE_UTILS_H_
