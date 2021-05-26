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
//
// Utility for UT use. Do NOT use it in production code.

#ifndef FLARE_FIBER_DETAIL_TESTING_H_
#define FLARE_FIBER_DETAIL_TESTING_H_

//////////////////////////////////////////////
// Mostly used by flare.fiber internally.   //
//                                          //
// For non-flare developers, consider using //
// `flare/testing/main.h` instead.          //
//////////////////////////////////////////////

#include <atomic>
#include <thread>
#include <utility>

#include "flare/fiber/detail/fiber_entity.h"
#include "flare/fiber/detail/scheduling_group.h"
#include "flare/fiber/fiber.h"
#include "flare/fiber/runtime.h"

namespace flare::fiber::testing {

template <class F>
void RunAsFiber(F&& f) {
  fiber::StartRuntime();
  std::atomic<bool> done{};
  Fiber([&, f = std::forward<F>(f)] {
    f();
    done = true;
  }).detach();
  while (!done) {
    std::this_thread::sleep_for(std::chrono::microseconds(1));
  }
  fiber::TerminateRuntime();
}

template <class F>
void StartFiberEntityInGroup(detail::SchedulingGroup* sg, bool system_fiber,
                             F&& f) {
  auto fiber = detail::CreateFiberEntity(sg, system_fiber, std::forward<F>(f));
  fiber->scheduling_group_local = false;
  sg->ReadyFiber(fiber, {});
}

}  // namespace flare::fiber::testing

#endif  // FLARE_FIBER_DETAIL_TESTING_H_
