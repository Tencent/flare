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

#include "flare/fiber/detail/stack_allocator.h"

#include <malloc.h>  // `memalign`.
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "gflags/gflags.h"

#include "flare/base/align.h"
#include "flare/base/internal/annotation.h"
#include "flare/base/logging.h"
#include "flare/base/never_destroyed.h"
#include "flare/base/object_pool.h"

using namespace std::literals;

// Both of the following flag affect user stack only.
DEFINE_int32(flare_fiber_stack_size, 131072,
             "Fiber stack size, in bytes. For the moment this flag cannot be "
             "changed dynamically, or you'll mess up with the virtual space.");
DEFINE_bool(flare_fiber_stack_enable_guard_page, true,
            "Place a guard page below each fiber stack. This reduce stack size "
            "by a page. Note that by default Linux expose a 64K limit on the "
            "number of total memory regions, therefore in case there are too "
            "many active fibers, enabling this option may reach that limit and "
            "crash the program. The aforementioned limit can be increased via "
            "`vm.max_map_count`. For the moment this flag cannot be changed "
            "dynamically, or you'll mess up with the virtual space.");

namespace flare::fiber::detail {

const auto kPageSize = getpagesize();

constexpr auto kOutOfMemoryError =
    "Cannot create guard page below fiber stack. Check `/proc/[pid]/maps` to "
    "see if there are too many memory regions. There's a limit at around 64K "
    "by default. If you reached the limit, try either disabling guard page or "
    "increasing `vm.max_map_count` (suggested).";

// All stacks (whether system stack or user stack) are registered here. This is
// necessary for our GDB plugin to find all the stacks.
//
// Only _actual_ stack allocation / deallocation needs to touch this. For
// allocations / deallocations covered by our object pool, they're irrelevant
// here.
//
// Registration / deregistration can be slow. But that's okay as it's already
// slow enough to _actually_ creating / destroying stacks. These operations
// incur heavy VMA operations.
struct StackRegistry {
  // Listed as public as they're our "public" interfaces to GDB plugin.
  //
  // Code in this TU should use methods below instead of touching these fields.
  void** stacks = nullptr;  // Leaked on exit. Doesn't matter.
  std::size_t used = 0;
  std::size_t capacity = 0;

  // Register a newly-allocated stack.
  //
  // `ptr` should point to stack bottom (i.e. one byte past the stack region).
  // That's where our fiber control block (GDB plugin need it) resides.
  void RegisterStack(void* ptr) {
    std::scoped_lock _(lock_);  // It's slow, so be it.
    ++used;
    auto slot = UnsafeFindSlotOf(nullptr);
    if (slot) {
      *slot = ptr;
      return;
    }

    UnsafeResizeRegistry();
    *UnsafeFindSlotOf(nullptr) = ptr;  // Must succeed this time.
  }

  // Deregister a going-to-be-freed stack. `ptr` points to stack bottom.
  void DeregisterStack(void* ptr) {
    std::scoped_lock _(lock_);

    flare::ScopedDeferred __([&] {
      // If `stacks` is too large we should consider shrinking it.
      if (capacity > 1024 && capacity / 2 > used) {
        UnsafeShrinkRegistry();
      }
    });

    --used;
    if (auto p = UnsafeFindSlotOf(ptr)) {
      *p = nullptr;
      return;
    }
    FLARE_UNREACHABLE("Unrecognized stack {}.", ptr);
  }

 private:
  void** UnsafeFindSlotOf(void* ptr) {
    for (int i = 0; i != capacity; ++i) {
      if (stacks[i] == ptr) {
        return &stacks[i];
      }
    }
    return nullptr;
  }

  void UnsafeShrinkRegistry() {
    auto new_capacity = capacity / 2;
    FLARE_CHECK(new_capacity);
    auto new_stacks = new void*[new_capacity];
    auto copied = 0;

    memset(new_stacks, 0, new_capacity * sizeof(void*));
    for (int i = 0; i != capacity; ++i) {
      if (stacks[i]) {
        new_stacks[copied++] = stacks[i];
      }
    }

    FLARE_CHECK_EQ(copied, used);
    FLARE_CHECK_LE(copied, new_capacity);
    capacity = new_capacity;
    delete[] std::exchange(stacks, new_stacks);
  }

  void UnsafeResizeRegistry() {
    if (capacity == 0) {  // We haven't been initialized yet.
      capacity = 8;
      stacks = new void*[capacity];
      memset(stacks, 0, sizeof(void*) * capacity);
    } else {
      auto new_capacity = capacity * 2;
      auto new_stacks = new void*[new_capacity];
      memset(new_stacks, 0, new_capacity * sizeof(void*));
      memcpy(new_stacks, stacks, capacity * sizeof(void*));
      capacity = new_capacity;
      delete[] std::exchange(stacks, new_stacks);
    }
  }

 private:
  std::mutex lock_;
} stack_registry;  // Using global variable here. This makes looking up this
                   // variable easy in GDB plugin.

inline std::size_t GetBias() {
  return FLAGS_flare_fiber_stack_enable_guard_page ? kPageSize : 0;
}

inline std::size_t GetAllocationSize() {
  // `DEFINE_validator` is not supported by our gflags, unfortunately.
  FLARE_CHECK(FLAGS_flare_fiber_stack_size % kPageSize == 0,
              "UserStack size ({}) must be a multiple of page size ({}).",
              FLAGS_flare_fiber_stack_size, kPageSize);

  return FLAGS_flare_fiber_stack_size + GetBias();
}

UserStack* CreateUserStackImpl() {
  auto p = mmap(nullptr, GetAllocationSize(), PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, 0, 0);
  FLARE_LOG_FATAL_IF(p == nullptr, "{}", kOutOfMemoryError);
  FLARE_CHECK_EQ(reinterpret_cast<std::uintptr_t>(p) % kPageSize, 0);
  if (FLAGS_flare_fiber_stack_enable_guard_page) {
    FLARE_LOG_FATAL_IF(mprotect(p, kPageSize, PROT_NONE) != 0, "{}",
                       kOutOfMemoryError);
  }

  // Actual start (lowest address) of the stack.
  auto stack = reinterpret_cast<char*>(p) + GetBias();
  // One byte past the stack region.
  auto stack_bottom = stack + FLAGS_flare_fiber_stack_size;

  // Register the stack.
  stack_registry.RegisterStack(stack_bottom);

  // Give it back to the caller.
  return reinterpret_cast<UserStack*>(stack);
}

void DestroyUserStackImpl(UserStack* ptr) {
  FLARE_CHECK(reinterpret_cast<std::uintptr_t>(ptr) % kPageSize == 0);

  // Remove the stack from our registry.
  auto stack_bottom =
      reinterpret_cast<char*>(ptr) + FLAGS_flare_fiber_stack_size;
  stack_registry.DeregisterStack(stack_bottom);

  FLARE_PCHECK(munmap(reinterpret_cast<char*>(ptr) - GetBias(),
                      GetAllocationSize()) == 0);
}

SystemStack* CreateSystemStackImpl() {
  // `alignof(FiberEntity)`. Hardcoded to avoid introducing dependency to
  // `fiber_entity.h`.
  constexpr auto kAlign = hardware_destructive_interference_size;

  // Rather simple.. Memory allocator should handle it well. We don't even have
  // to make it aligned to page boundary.

  // Using POSIX `memalign` here, C++17 `aligned_alloc` is not available on
  // CentOS 6.
  auto stack = memalign(kAlign, kSystemStackSize);
  FLARE_CHECK(reinterpret_cast<std::uintptr_t>(stack) % kAlign == 0);
  auto stack_bottom = reinterpret_cast<char*>(stack) + kSystemStackSize;

  // Register it and return.
  stack_registry.RegisterStack(stack_bottom);
  return reinterpret_cast<SystemStack*>(stack);
}

void DestroySystemStackImpl(SystemStack* ptr) {
  auto stack_bottom = reinterpret_cast<char*>(ptr) + kSystemStackSize;
  stack_registry.DeregisterStack(stack_bottom);
  free(ptr);  // Let it go.
}

}  // namespace flare::fiber::detail
