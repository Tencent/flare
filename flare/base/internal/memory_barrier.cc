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

#include "flare/base/internal/memory_barrier.h"

#include <sys/mman.h>

#include <mutex>

#include "flare/base/internal/logging.h"
#include "flare/base/never_destroyed.h"

namespace flare::internal {

namespace {

void* CreateOneByteDummyPage() {
  auto ptr = mmap(nullptr, 1, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  FLARE_PCHECK(ptr, "Cannot create dummy page for asymmetric memory barrier.");
  (void)mlock(ptr, 1);
  return ptr;
}

// `membarrier()` is not usable until Linux 4.3, which is not available until
// tlinux 3 is deployed.
//
// Here Folly provides a workaround by mutating our page tables. Mutating page
// tables, for the moment, implicitly cause the system to execute a barrier on
// every core running our threads. I shamelessly copied their solution here.
//
// @sa:
// https://github.com/facebook/folly/blob/master/folly/synchronization/AsymmetricMemoryBarrier.cpp
void HomemadeMembarrier() {
  static void* dummy_page = CreateOneByteDummyPage();  // Never freed.

  // Previous memory accesses may not be reordered after syscalls below.
  // (Frankly this is implied by acquired lock below.)
  MemoryBarrier();

  static NeverDestroyed<std::mutex> lock;
  std::scoped_lock _(*lock);

  // Upgrading protection does not always result in fence in each core (as it
  // can be delayed until #PF).
  FLARE_PCHECK(mprotect(dummy_page, 1, PROT_READ | PROT_WRITE) == 0);
  *static_cast<char*>(dummy_page) = 0;  // Make sure the page is present.
  // This time a barrier should be issued to every cores.
  FLARE_PCHECK(mprotect(dummy_page, 1, PROT_READ) == 0);

  // Subsequent memory accesses may not be reordered before syscalls above. (Not
  // sure if it's already implied by `mprotect`?)
  MemoryBarrier();
}

}  // namespace

void AsymmetricBarrierHeavy() {
  // TODO(luobogao): Use `membarrier()` if available.
  HomemadeMembarrier();
}

}  // namespace flare::internal
