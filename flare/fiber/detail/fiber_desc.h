// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef FLARE_FIBER_DETAIL_FIBER_DESC_H_
#define FLARE_FIBER_DETAIL_FIBER_DESC_H_

#include "flare/base/align.h"
#include "flare/base/function.h"
#include "flare/base/ref_ptr.h"
#include "flare/fiber/detail/runnable_entity.h"

namespace flare::fiber::detail {

class ExitBarrier;

// This structure stores information describing how to instantiate a
// `FiberEntity`. The instantiation is deferred to first run of the fiber.
//
// This approach should help performance since:
//
// - Reduced memory footprint: We don't need to allocate a stack until actual
//   run.
//
// - Alleviated producer-consumer effect: The fiber stack is allocated in fiber
//   worker, where most (exited) fiber' stack is freed. This promotes more
//   thread-local-level reuse. If we keep allocating stack from thread X and
//   consume it in thread Y, we'd have a hard time in transfering fiber stack
//   between them (mostly because we can't afford a big transfer-size to avoid
//   excessive memory footprint.).
struct alignas(hardware_destructive_interference_size) FiberDesc
    : RunnableEntity {
  Function<void()> start_proc;
  RefPtr<ExitBarrier> exit_barrier;
  std::uint64_t last_ready_tsc;
  bool scheduling_group_local;
  bool system_fiber;

  FiberDesc();
};

// Creates a new fiber startup descriptor.
FiberDesc* NewFiberDesc() noexcept;

// Destroys a fiber startup descriptor.
//
// In most cases this method is called by `InstantiateFiberEntity`. Calling this
// method yourself is almost always an error.
void DestroyFiberDesc(FiberDesc* desc) noexcept;

}  // namespace flare::fiber::detail

#endif  // FLARE_FIBER_DETAIL_FIBER_DESC_H_
