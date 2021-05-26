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

#ifndef FLARE_FIBER_ASYNC_H_
#define FLARE_FIBER_ASYNC_H_

#include <type_traits>
#include <utility>

#include "flare/base/future.h"
#include "flare/fiber/execution_context.h"
#include "flare/fiber/fiber.h"

namespace flare::fiber {

// Runs `f` with `args...` asynchronously.
//
// It's unspecified in which fiber (except the caller's own one) `f` is called.
//
// Note that this method is only available in fiber runtime. If you want to
// start a fiber from pthread, use `StartFiberFromPthread` (@sa: `fiber.h`)
// instead.
template <class F, class... Args,
          class R = future::futurize_t<std::invoke_result_t<F&&, Args&&...>>>
R Async(Launch policy, std::size_t scheduling_group,
        ExecutionContext* execution_context, F&& f, Args&&... args) {
  FLARE_CHECK(policy == Launch::Post || policy == Launch::Dispatch);
  future::as_promise_t<R> p;
  auto rc = p.GetFuture();

  // @sa: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0780r2.html
  auto proc = [p = std::move(p), f = std::forward<F>(f),
               args = std::make_tuple(std::forward<Args>(args)...)]() mutable {
    if constexpr (std::is_same_v<Future<>, R>) {
      std::apply(f, std::move(args));
      p.SetValue();
    } else {
      p.SetValue(std::apply(f, std::move(args)));
    }
  };
  internal::StartFiberDetached(
      Fiber::Attributes{.launch_policy = policy,
                        .scheduling_group = scheduling_group,
                        .execution_context = execution_context},
      std::move(proc));
  return rc;
}

template <class F, class... Args,
          class R = future::futurize_t<std::invoke_result_t<F&&, Args&&...>>>
R Async(Launch policy, std::size_t scheduling_group, F&& f, Args&&... args) {
  return Async(policy, scheduling_group, ExecutionContext::Current(),
               std::forward<F>(f), std::forward<Args>(args)...);
}

template <class F, class... Args,
          class R = future::futurize_t<std::invoke_result_t<F&&, Args&&...>>>
R Async(Launch policy, F&& f, Args&&... args) {
  return Async(policy, Fiber::kNearestSchedulingGroup, std::forward<F>(f),
               std::forward<Args>(args)...);
}

template <class F, class... Args,
          class = std::enable_if_t<std::is_invocable_v<F&&, Args&&...>>>
auto Async(F&& f, Args&&... args) {
  return Async(Launch::Post, std::forward<F>(f), std::forward<Args>(args)...);
}

}  // namespace flare::fiber

#endif  // FLARE_FIBER_ASYNC_H_
