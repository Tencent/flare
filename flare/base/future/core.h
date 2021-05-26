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

#ifndef FLARE_BASE_FUTURE_CORE_H_
#define FLARE_BASE_FUTURE_CORE_H_

#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

#include "flare/base/function.h"
#include "flare/base/future/boxed.h"
#include "flare/base/future/executor.h"
#include "flare/base/internal/logging.h"

namespace flare::future {

// This is the shared state between `Promise<...>` and `Future<...>`.
//
// `Core` itself will do necessary synchronizations to be multithread safe.
//
// Unfortunately `intrusive_ptr` is not in STL yet (@sa P0468R0), thus we're
// using `std::shared_ptr<...>` to hold it.
//
// Seastar rolled their own `lw_shared_ptr`, which I believe removed some
// abilities provided by STL such as custom deleter, and thus more lightweight.
//
// We don't need those abilities either, so it might be better for us to roll
// our own `lw_shared_ptr` as well once the overhead of these features become a
// bottleneck;
//
// TODO(luobogao): Now that we have `RefPtr<T>`, let's use it to substitute the
// use of `std::shared_ptr`s.
template <class... Ts>
class Core {
 public:
  using value_type = Boxed<Ts...>;
  using action_type = Function<void(value_type&&) noexcept>;

  // Construct a `Core` using `executor`.
  explicit Core(Executor executor) : executor_(std::move(executor)) {}

  // Satisfy the `Core` with the boxed value.
  //
  // Precondition: The `Core` must have not been satisfied.
  void SetBoxed(value_type&& value) noexcept;

  // Chain an action. It might be immediately invoked if the `Core` has
  // already been satisfied.
  //
  // At most 1 action may ever be chained for a given `Core`.
  void ChainAction(action_type&& action) noexcept;

  // Get executor used when invoking the continuation.
  Executor GetExecutor() const noexcept;

 private:
  // `Core` is not satisfied, the `continuation` (if any) is stored in the
  // state.
  struct WaitingForSingleObject {
    action_type on_satisfied;
  };
  // `Core` is satisfied, the value satisfied it is stored.
  struct Satisfied {
    value_type value;
    bool ever_called_continuation{false};
  };

  // I personally **think** a spin lock would work better here as there's
  // hardly any contention on a given `Core` likely to occur.
  //
  // But pthread's mutex does spin for some rounds before trapping, so I
  // *think* it should work just equally well from performance perspective.
  //
  // Not sure if HLE (Hardware Lock Elision) helps here, but it should.
  std::mutex lock_;
  std::variant<WaitingForSingleObject, Satisfied> state_;
  Executor executor_;
};

// Implementation goes below.

template <class... Ts>
void Core<Ts...>::SetBoxed(value_type&& value) noexcept {
  std::unique_lock<std::mutex> lk(lock_);

  // Transit to `Satisfied`.
  action_type action =
      std::move(std::get<WaitingForSingleObject>(state_).on_satisfied);
  state_.template emplace<Satisfied>(Satisfied{std::move(value), !!action});

  if (action) {
    // The `Core` has reached the final state, and won't be mutate any more
    // (except for its destruction), thus unlocking here is safe.
    lk.unlock();
    executor_.Execute(
        [a = std::move(action),
         v = std::move(std::get<Satisfied>(state_).value)]() mutable noexcept {
          a(std::move(v));
        });
  }  // Otherwise the lock will be release automatically.
}

template <class... Ts>
void Core<Ts...>::ChainAction(action_type&& action) noexcept {
  std::unique_lock<std::mutex> lk(lock_);

  if (state_.index() == 0) {  // We're still waiting for value.
    state_.template emplace<WaitingForSingleObject>(
        WaitingForSingleObject{std::move(action)});
  } else {  // Already satisfied, call the `action` immediately.
    FLARE_CHECK_EQ(state_.index(), 1);
    auto&& s = std::get<Satisfied>(state_);

    FLARE_CHECK(!s.ever_called_continuation,
                "Action may not be chained for multiple times.");

    s.ever_called_continuation = true;
    lk.unlock();  // Final state reached.

    executor_.Execute(
        [a = std::move(action), v = std::move(s.value)]() mutable noexcept {
          a(std::move(v));
        });
  }
}

template <class... Ts>
inline Executor Core<Ts...>::GetExecutor() const noexcept {
  return executor_;
}

}  // namespace flare::future

#endif  // FLARE_BASE_FUTURE_CORE_H_
