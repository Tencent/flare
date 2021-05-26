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

#ifndef FLARE_FIBER_FUTURE_H_
#define FLARE_FIBER_FUTURE_H_

#include <utility>

#include "flare/base/delayed_init.h"
#include "flare/base/future.h"
#include "flare/base/internal/time_view.h"
#include "flare/fiber/detail/waitable.h"

namespace flare::fiber {

// Analogous to `future::BlockingGet`, but this one won't block the underlying
// pthread.
template <class... Ts>
auto BlockingGet(Future<Ts...>&& f) {
  detail::Event ev;
  DelayedInit<future::Boxed<Ts...>> receiver;

  // Once the `future` is satisfied, our continuation will move the
  // result into `receiver` and notify `cv` to wake us up.
  std::move(f).Then([&](future::Boxed<Ts...> boxed) noexcept {
    // Do we need some synchronization primitives here to protect `receiver`?
    // `Event` itself guarantees `Set()` happens-before `Wait()` below, so
    // writing to `receiver` is guaranteed to happens-before reading it.
    //
    // But OTOH, what guarantees us that initialization of `receiver`
    // happens-before our assignment to it? `Future::Then`?
    receiver.Init(std::move(boxed));
    ev.Set();
  });

  // Block until our continuation wakes us up.
  ev.Wait();
  return std::move(*receiver).Get();
}

// Same as `BlockingGet` but this one accepts a timeout.
template <class... Ts>
auto BlockingTryGet(Future<Ts...>&& future,
                    const flare::internal::SteadyClockView& timeout) {
  struct State {
    detail::OneshotTimedEvent event;

    // Protects `receiver`.
    //
    // Unlike `BlockingGet`, here it's possible that after `event.Wait()` times
    // out, concurrently the future is satisfied. In this case the continuation
    // of the future will be racing with us on `receiver`.
    std::mutex lock;
    std::optional<future::Boxed<Ts...>> receiver;

    explicit State(std::chrono::steady_clock::time_point expires_at)
        : event(expires_at) {}
  };
  auto state = std::make_shared<State>(timeout.Get());

  // `state` must be copied here, in case of timeout, we'll leave the scope
  // before continuation is fired.
  std::move(future).Then([state](future::Boxed<Ts...> boxed) noexcept {
    std::scoped_lock _(state->lock);
    state->receiver.emplace(std::move(boxed));
    state->event.Set();
  });

  state->event.Wait();
  std::scoped_lock _(state->lock);
  if constexpr (sizeof...(Ts) == 0) {
    return !!state->receiver;
  } else {
    return state->receiver ? std::optional(std::move(*state->receiver).Get())
                           : std::nullopt;
  }
}

template <class... Ts>
auto BlockingGet(Future<Ts...>* f) {
  return fiber::BlockingGet(std::move(*f));
}

template <class... Ts>
auto BlockingTryGet(Future<Ts...>* f,
                    const flare::internal::SteadyClockView& timeout) {
  return fiber::BlockingTryGet(std::move(*f), timeout);
}

}  // namespace flare::fiber

#endif  // FLARE_FIBER_FUTURE_H_
