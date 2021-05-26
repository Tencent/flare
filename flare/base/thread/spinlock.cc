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

#include "flare/base/thread/spinlock.h"

#include <chrono>

using namespace std::literals;

#if defined(__x86_64__)
#define FLARE_CPU_RELAX() asm volatile("pause" ::: "memory")
#else
// TODO(luobogao): Implement this macro for non-x86 ISAs.
#define FLARE_CPU_RELAX()
#endif

namespace flare {

// @s: https://code.woboq.org/userspace/glibc/nptl/pthread_spin_lock.c.html
void Spinlock::LockSlow() noexcept {
  do {
    // Test ...
    while (locked_.load(std::memory_order_relaxed)) {
      FLARE_CPU_RELAX();
    }

    // ... and set.
  } while (locked_.exchange(true, std::memory_order_acquire));
}

}  // namespace flare
