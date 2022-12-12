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

#ifndef FLARE_FIBER_THIS_FIBER_H_
#define FLARE_FIBER_THIS_FIBER_H_

#include <chrono>

#include "flare/base/chrono.h"
#include "flare/fiber/fiber.h"

namespace flare::this_fiber {

// Yield execution.
//
// If there's no other fiber is ready to run, the caller will be rescheduled
// immediately.
void Yield();

// Block calling fiber until `expires_at`.
void SleepUntil(std::chrono::steady_clock::time_point expires_at);

// Block calling fiber for `expires_in`.
void SleepFor(std::chrono::nanoseconds expires_in);

// `SleepUntil` for clocks other than `std::steady_clock`.
template <class Clock, class Duration>
void SleepUntil(std::chrono::time_point<Clock, Duration> expires_at) {
  return SleepUntil(ReadSteadyClock() + (expires_at - Clock::now()));
}

// `SleepFor` for durations other than `std::chrono::nanoseconds`.
template <class Rep, class Period>
void SleepFor(std::chrono::duration<Rep, Period> expires_in) {
  return SleepFor(static_cast<std::chrono::nanoseconds>(expires_in));
}

// Returns fiber ID of the calling fiber.
Fiber::Id GetId();

}  // namespace flare::this_fiber

#endif  // FLARE_FIBER_THIS_FIBER_H_
