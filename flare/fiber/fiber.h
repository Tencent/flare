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

#ifndef FLARE_FIBER_FIBER_H_
#define FLARE_FIBER_FIBER_H_

#include <limits>
#include <utility>
#include <vector>

#include "flare/base/function.h"
#include "flare/base/ref_ptr.h"

namespace flare {

namespace fiber::detail {

class ExitBarrier;

}  // namespace fiber::detail

namespace fiber {

enum class Launch {
  Post,
  Dispatch  // If possible, yield current pthread worker to user's code.
};

class ExecutionContext;

}  // namespace fiber

// Analogous to `std::thread`, but it's for fiber.
//
// Directly constructing `Fiber` does NOT propagate execution context. Consider
// using `fiber::Async` instead.
class Fiber {
 public:
  // Hopefully you don't start 2**64 scheduling groups.
  static constexpr auto kNearestSchedulingGroup =
      std::numeric_limits<std::size_t>::max() - 1;
  static constexpr auto kUnspecifiedSchedulingGroup =
      std::numeric_limits<std::size_t>::max();

  using Id = struct InternalOpaqueId*;

  struct Attributes {
    // How the fiber is launched.
    fiber::Launch launch_policy = fiber::Launch::Post;

    // Which scheduling group should the fiber be *initially* placed in. Note
    // that unless you also have `scheduling_group_local` set, the fiber can be
    // stolen by workers belonging to other scheduling group.
    std::size_t scheduling_group = kNearestSchedulingGroup;

    // If set, fiber's start procedure is run in this execution context.
    //
    // `Fiber` will take a reference to the execution context, so you're safe to
    // release your own ref. once `Fiber` is constructed.
    fiber::ExecutionContext* execution_context = nullptr;

    // If set, this fiber is treated as system fiber. Certain restrictions may
    // apply to system fiber (e.g., stack size.).
    //
    // This flag is reserved for internal use only.
    bool system_fiber = false;

    // If set, `scheduling_group` is enforced. (i.e., work-stealing is disabled
    // on this fiber.)
    bool scheduling_group_local = false;

    // TODO(luobogao): `bool start_in_detached_fashion`. If set, the fiber is
    // immediately detached once created. This provide us further optimization
    // possibility.

    // `name`?
  };

  // Create an empty (invalid) fiber.
  Fiber();

  // Create a fiber with attributes. It will run from `start`.
  Fiber(const Attributes& attr, Function<void()>&& start);

  // Create fiber by calling `f` with args.
  template <class F, class... Args,
            class = std::enable_if_t<std::is_invocable_v<F&&, Args&&...>>>
  explicit Fiber(F&& f, Args&&... args)
      : Fiber(Attributes(), std::forward<F>(f), std::forward<Args>(args)...) {}

  // Create fiber by calling `f` with args, using policy `policy`.
  template <class F, class... Args,
            class = std::enable_if_t<std::is_invocable_v<F&&, Args&&...>>>
  Fiber(fiber::Launch policy, F&& f, Args&&... args)
      : Fiber(Attributes{.launch_policy = policy}, std::forward<F>(f),
              std::forward<Args>(args)...) {}

  // Create fiber by calling `f` with args, using attributes `attr`.
  template <class F, class... Args,
            class = std::enable_if_t<std::is_invocable_v<F&&, Args&&...>>>
  Fiber(const Attributes& attr, F&& f, Args&&... args)
      : Fiber(attr,
              [f = std::forward<F>(f),
               // P0780R2 is not implemented as of now (GCC 8.2).
               t = std::make_tuple(std::forward<Args>(args)...)]() mutable {
                std::apply(std::move(f), std::move(t));
              }) {}

  // Special case if no parameter is passed to `F`, in this case we don't need
  // an indirection (the extra lambda).
  template <class F, class = std::enable_if_t<std::is_invocable_v<F&&>>>
  Fiber(const Attributes& attr, F&& f)
      : Fiber(attr, Function<void()>(std::forward<F>(f))) {}

  // If a `Fiber` object which owns a fiber is destructed with no prior call to
  // `join()` or `detach()`, it leads to abort.
  ~Fiber();

  // Detach the fiber.
  void detach();

  // Wait for the fiber to exit.
  void join();

  // Test if we can call `join()` on this object.
  bool joinable() const;

  // Movable but not copyable
  Fiber(Fiber&&) noexcept;
  Fiber& operator=(Fiber&&) noexcept;

 private:
  RefPtr<fiber::detail::ExitBarrier> join_impl_;
};

// In certain cases you may want to start a fiber from pthread environment, so
// that your code can use fiber primitives. This method helps you accomplish
// this.
void StartFiberFromPthread(Function<void()>&& start_proc);

namespace fiber::internal {

// Start a new fiber in "detached" state. This method performs better than
// `Fiber(...).detach()` in trade of simple interface.
//
// Introduced for perf. reasons, for internal use only.
void StartFiberDetached(Function<void()>&& start_proc);
void StartSystemFiberDetached(Function<void()>&& start_proc);
void StartFiberDetached(Fiber::Attributes&& attrs,
                        Function<void()>&& start_proc);

// Start fibers in batch, in "detached" state.
void BatchStartFiberDetached(std::vector<Function<void()>>&& start_procs);

}  // namespace fiber::internal

}  // namespace flare

#endif  // FLARE_FIBER_FIBER_H_
