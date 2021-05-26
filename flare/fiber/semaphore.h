// Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef FLARE_FIBER_SEMAPHORE_H_
#define FLARE_FIBER_SEMAPHORE_H_

#include <chrono>
#include <cinttypes>
#include <limits>
#include <type_traits>

#include "flare/base/thread/semaphore.h"
#include "flare/fiber/condition_variable.h"
#include "flare/fiber/mutex.h"

namespace flare::fiber {

// Same as `flare::CountingSemaphore` except that this one is for fiber. That
// is, this only only blocks the calling fiber, but not the underlying pthread.
template <std::ptrdiff_t kLeastMaxValue =
              std::numeric_limits<std::uint32_t>::max()>
class CountingSemaphore
    : public flare::BasicCountingSemaphore<
          fiber::Mutex, fiber::ConditionVariable, kLeastMaxValue> {
 public:
  explicit CountingSemaphore(std::ptrdiff_t desired)
      : BasicCountingSemaphore<fiber::Mutex, fiber::ConditionVariable,
                               kLeastMaxValue>(desired) {}
};

using BinarySemaphore = CountingSemaphore<1>;

}  // namespace flare::fiber

#endif  // FLARE_FIBER_SEMAPHORE_H_
