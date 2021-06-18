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

#include "flare/fiber/fiber.h"

#include <utility>
#include <vector>

#include "flare/base/likely.h"
#include "flare/base/random.h"
#include "flare/fiber/detail/fiber_entity.h"
#include "flare/fiber/detail/scheduling_group.h"
#include "flare/fiber/detail/waitable.h"
#include "flare/fiber/execution_context.h"
#include "flare/fiber/runtime.h"

namespace flare {

namespace {

fiber::detail::SchedulingGroup* GetSchedulingGroup(std::size_t id) {
  if (FLARE_LIKELY(id == Fiber::kNearestSchedulingGroup)) {
    return fiber::detail::NearestSchedulingGroup();
  } else if (id == Fiber::kUnspecifiedSchedulingGroup) {
    return fiber::detail::GetSchedulingGroup(
        Random<std::size_t>(0, fiber::GetSchedulingGroupCount() - 1));
  } else {
    return fiber::detail::GetSchedulingGroup(id);
  }
}

}  // namespace

Fiber::Fiber() = default;

Fiber::~Fiber() {
  FLARE_CHECK(!joinable(),
              "You need to call either `join()` or `detach()` prior to destroy "
              "a fiber.");
}

Fiber::Fiber(const Attributes& attr, Function<void()>&& start) {
  // Choose a scheduling group for running this fiber.
  auto sg = GetSchedulingGroup(attr.scheduling_group);
  FLARE_CHECK(sg, "No scheduling group is available?");

  if (attr.execution_context) {
    // Caller specified an execution context, so we should wrap `start` to run
    // in the execution context.
    //
    // `ec` holds a reference to `attr.execution_context`, it's released when
    // `start` returns.
    start = [start = std::move(start),
             ec = RefPtr(ref_ptr, attr.execution_context)] {
      ec->Execute(start);
    };
  }
  // `desc` will cease to exist once `start` returns. We don't own it.
  auto desc = fiber::detail::NewFiberDesc();
  desc->start_proc = std::move(start);
  desc->scheduling_group_local = attr.scheduling_group_local;
  desc->system_fiber = attr.system_fiber;

  // If `join()` is called, we'll sleep on this.
  desc->exit_barrier = object_pool::GetRefCounted<fiber::detail::ExitBarrier>();
  join_impl_ = desc->exit_barrier;

  // Schedule the fiber.
  if (attr.launch_policy == fiber::Launch::Post) {
    sg->StartFiber(desc);
  } else {
    sg->SwitchTo(fiber::detail::GetCurrentFiberEntity(),
                 fiber::detail::InstantiateFiberEntity(sg, desc));
  }
}

void Fiber::detach() {
  FLARE_CHECK(joinable(), "The fiber is in detached state.");
  join_impl_ = nullptr;
}

void Fiber::join() {
  FLARE_CHECK(joinable(), "The fiber is in detached state.");
  join_impl_->Wait();
  join_impl_.Reset();
}

bool Fiber::joinable() const { return !!join_impl_; }

Fiber::Fiber(Fiber&&) noexcept = default;
Fiber& Fiber::operator=(Fiber&&) noexcept = default;

void StartFiberFromPthread(Function<void()>&& start_proc) {
  fiber::internal::StartFiberDetached(std::move(start_proc));
}

namespace fiber::internal {

void StartFiberDetached(Function<void()>&& start_proc) {
  auto desc = detail::NewFiberDesc();
  desc->start_proc = std::move(start_proc);
  FLARE_CHECK(!desc->exit_barrier);
  desc->scheduling_group_local = false;
  desc->system_fiber = false;

  fiber::detail::NearestSchedulingGroup()->StartFiber(desc);
}

void StartSystemFiberDetached(Function<void()>&& start_proc) {
  auto desc = detail::NewFiberDesc();
  desc->start_proc = std::move(start_proc);
  FLARE_CHECK(!desc->exit_barrier);
  desc->scheduling_group_local = false;
  desc->system_fiber = true;

  fiber::detail::NearestSchedulingGroup()->StartFiber(desc);
}

void StartFiberDetached(Fiber::Attributes&& attrs,
                        Function<void()>&& start_proc) {
  auto sg = GetSchedulingGroup(attrs.scheduling_group);

  if (attrs.execution_context) {
    start_proc = [start_proc = std::move(start_proc),
                  ec = RefPtr(ref_ptr, std::move(attrs.execution_context))] {
      ec->Execute(start_proc);
    };
  }

  auto desc = detail::NewFiberDesc();
  desc->start_proc = std::move(start_proc);
  FLARE_CHECK(!desc->exit_barrier);
  desc->scheduling_group_local = attrs.scheduling_group_local;
  desc->system_fiber = attrs.system_fiber;

  if (attrs.launch_policy == fiber::Launch::Post) {
    sg->StartFiber(desc);
  } else {
    sg->SwitchTo(fiber::detail::GetCurrentFiberEntity(),
                 detail::InstantiateFiberEntity(sg, desc));
  }
}

void BatchStartFiberDetached(std::vector<Function<void()>>&& start_procs) {
  std::vector<fiber::detail::FiberDesc*> descs;
  for (auto&& e : start_procs) {
    auto desc = fiber::detail::NewFiberDesc();
    desc->start_proc = std::move(e);
    FLARE_CHECK(!desc->exit_barrier);
    desc->scheduling_group_local = false;
    desc->system_fiber = false;
    descs.push_back(desc);
  }

  fiber::detail::NearestSchedulingGroup()->StartFibers(
      descs.data(), descs.data() + descs.size());
}

}  // namespace fiber::internal

}  // namespace flare
