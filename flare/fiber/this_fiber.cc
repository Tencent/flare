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

#include "flare/fiber/this_fiber.h"

#include "flare/fiber/detail/fiber_entity.h"
#include "flare/fiber/detail/scheduling_group.h"
#include "flare/fiber/detail/waitable.h"

namespace flare::this_fiber {

Fiber::Id GetId() {
  auto self = fiber::detail::GetCurrentFiberEntity();
  FLARE_CHECK(self,
              "this_fiber::GetId may only be called in fiber environment.");
  return reinterpret_cast<Fiber::Id>(self->exit_barrier.Get());
}

void Yield() {
  auto self = fiber::detail::GetCurrentFiberEntity();
  FLARE_CHECK(self,
              "this_fiber::Yield may only be called in fiber environment.");
  self->scheduling_group->Yield(self);
}

void SleepUntil(std::chrono::steady_clock::time_point expires_at) {
  fiber::detail::WaitableTimer wt(expires_at);
  wt.wait();
}

void SleepFor(std::chrono::nanoseconds expires_in) {
  SleepUntil(ReadSteadyClock() + expires_in);
}

}  // namespace flare::this_fiber
