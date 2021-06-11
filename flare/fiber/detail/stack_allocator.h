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

#ifndef FLARE_FIBER_DETAIL_STACK_ALLOCATOR_H_
#define FLARE_FIBER_DETAIL_STACK_ALLOCATOR_H_

#include <chrono>
#include <limits>
#include <utility>

#include "gflags/gflags_declare.h"

#include "flare/base/internal/annotation.h"
#include "flare/base/object_pool.h"

DECLARE_int32(flare_fiber_stack_size);

namespace flare::fiber::detail {

// Here we define two types of stacks:
//
// - User stack: Where user's code will be running on. Its size can be
//   controlled by `FLAGS_flare_fiber_stack_size`. It's also possible to enable
//   guard page (as by default) for this type of stacks.
//
//   However, creating such a stack does incur some overhead:
//
//   - To keep memory footprint low and not reach system limit on maximum number
//     of VMAs. This type of stack is not cached a lot per thread.
//
//   - Large stack size uses memory, as obvious.
//
//   - Allocating guard page requires a dedicated VMA.
//
// - System stack: This type of stack is used solely by the framework. It's size
//   is statically determined (@sa: `kSystemStackSize`), and no guard page is
//   provided.
//
//   Obviously, using stack of such type is more dangerous than "user stack".
//
//   However, it does brings us some good, notably the elimination of overhead
//   of user stack.

// Merely tagging types. They're used for specializing `PoolTrait<...>`.
//
// The real "object" represented by it is a fiber stack (contiguous pages
// allocated by `CreateUserStackImpl()`.).
struct UserStack {};
struct SystemStack {};

UserStack* CreateUserStackImpl();
void DestroyUserStackImpl(UserStack* ptr);

SystemStack* CreateSystemStackImpl();
void DestroySystemStackImpl(SystemStack* ptr);

// System stacks are used solely by us. It's our own responsibility not to
// overflow the stack. This restriction permits several optimization
// possibilities.
#ifdef FLARE_INTERNAL_USE_ASAN
constexpr auto kSystemStackPoisonedSize = 4096;
constexpr auto kSystemStackSize = 16384 + kSystemStackPoisonedSize;
#else
constexpr auto kSystemStackSize = 16384;
#endif

}  // namespace flare::fiber::detail

namespace flare {

template <>
struct PoolTraits<fiber::detail::UserStack> {
  using UserStack = fiber::detail::UserStack;

  static constexpr auto kType = PoolType::MemoryNodeShared;
  static constexpr auto kLowWaterMark = 512;
  // Don't set high water-mark too large, or we risk running out of
  // `vm.max_map_count`.
  static constexpr auto kHighWaterMark = 16384;
  static constexpr auto kMaxIdle = std::chrono::seconds(10);
  static constexpr auto kMinimumThreadCacheSize = 32;
  // If we allocate more stack than necessary, we're risking reaching
  // max_map_count limit.
  static constexpr auto kTransferBatchSize = 128;

  static auto Create() {
    auto ptr = fiber::detail::CreateUserStackImpl();

#ifdef FLARE_INTERNAL_USE_ASAN
    // Poisoned immediately. It's un-poisoned prior to use.
    flare::internal::asan::PoisonMemoryRegion(ptr,
                                              FLAGS_flare_fiber_stack_size);
#endif

    return ptr;
  }

  static void Destroy(UserStack* ptr) {
#ifdef FLARE_INTERNAL_USE_ASAN
    // Un-poisoned prior to free so as not to interference with other
    // allocations.
    flare::internal::asan::UnpoisonMemoryRegion(ptr,
                                                FLAGS_flare_fiber_stack_size);
#endif

    DestroyUserStackImpl(ptr);
  }

  // Canary value here? We've done this for system stack, where guard page is
  // not applicable.

#ifdef FLARE_INTERNAL_USE_ASAN

  // The stack is not "unpoison"-ed (if ASAN is in use) on get and re-poisoned
  // on put. This helps us to detect use-after-free of the stack as well.
  static void OnGet(UserStack* ptr) {
    flare::internal::asan::UnpoisonMemoryRegion(ptr,
                                                FLAGS_flare_fiber_stack_size);
  }

  static void OnPut(UserStack* ptr) {
    flare::internal::asan::PoisonMemoryRegion(ptr,
                                              FLAGS_flare_fiber_stack_size);
  }

#endif
};

template <>
struct PoolTraits<fiber::detail::SystemStack> {
  // There's no guard page for system stack. However, we still want to detect
  // stack overflow (in a limited fashion). Therefore, we put these canary
  // values at stack limit. If they got overwritten, stack overflow can be
  // detected at stack deallocation (in `OnPut`).
  //
  // EncodeHex("FlareStackCanary"): 466c617265537461636b43616e617279
  inline static constexpr std::uint64_t kStackCanary0 = 0x466c'6172'6553'7461;
  inline static constexpr std::uint64_t kStackCanary1 = 0x636b'4361'6e61'7279;

  using SystemStack = fiber::detail::SystemStack;

  static constexpr auto kType = PoolType::MemoryNodeShared;
  static constexpr auto kLowWaterMark = 4096;
  static constexpr auto kHighWaterMark =
      std::numeric_limits<std::size_t>::max();
  static constexpr auto kMaxIdle = std::chrono::seconds(10);
  static constexpr auto kMinimumThreadCacheSize = 128;
  static constexpr auto kTransferBatchSize = 512;

  static auto Create() {
    auto ptr = fiber::detail::CreateSystemStackImpl();

#ifndef FLARE_INTERNAL_USE_ASAN
    // Canary value is not of much use if ASan is enabled. In case of ASan, we
    // poison the bytes at stack limit. This is more powerful than checking
    // canary values on stack deallocation.
    InitializeCanaryValue(ptr);
#endif

#ifdef FLARE_INTERNAL_USE_ASAN
    // The memory region is poisoned immediately. We'll un-poison it prior to
    // use.
    flare::internal::asan::PoisonMemoryRegion(ptr,
                                              fiber::detail::kSystemStackSize);
#endif

    return ptr;
  }

  static void Destroy(SystemStack* ptr) {
#ifdef FLARE_INTERNAL_USE_ASAN
    // Un-poison these bytes before returning them to the runtime.
    flare::internal::asan::UnpoisonMemoryRegion(
        ptr, fiber::detail::kSystemStackSize);
#endif

    DestroySystemStackImpl(ptr);
  }

  static void OnGet(SystemStack* ptr) {
#ifndef FLARE_INTERNAL_USE_ASAN
    // Make sure our canary is still there. We only do this check if ASan is NOT
    // enabled. If ASan is enabled, these bytes are poisoned and any access to
    // then should have already trigger an error report.
    VerifyCanaryValue(ptr);
#endif

#ifdef FLARE_INTERNAL_USE_ASAN
    // The first bytes are left poisoned. They acts as the same role as "guard
    // page".
    auto&& [usable, limit] = SplitMemoryRegionForStack(ptr);
    flare::internal::asan::UnpoisonMemoryRegion(usable, limit);
#endif
  }

  static void OnPut(SystemStack* ptr) {
#ifndef FLARE_INTERNAL_USE_ASAN
    // Don't overflow our stack.
    VerifyCanaryValue(ptr);
#endif

#ifdef FLARE_INTERNAL_USE_ASAN
    // Re-poisoned so that we can detect use-after-free on stack.
    auto&& [usable, limit] = SplitMemoryRegionForStack(ptr);
    flare::internal::asan::PoisonMemoryRegion(usable, limit);
#endif
  }

  // `ptr` points to the first byte (i.e., stack limit) of the stack.
  static void InitializeCanaryValue(SystemStack* ptr) {
    static_assert(sizeof(std::uint64_t) == 8);

    auto canaries = reinterpret_cast<volatile std::uint64_t*>(ptr);
    canaries[0] = kStackCanary0;
    canaries[1] = kStackCanary1;
  }

  static void VerifyCanaryValue(SystemStack* ptr) {
    auto canaries = reinterpret_cast<std::uint64_t*>(ptr);  // U.B.?
    FLARE_CHECK_EQ(
        kStackCanary0, canaries[0],
        "The first canary value was overwritten. The stack is corrupted.");
    FLARE_CHECK_EQ(
        kStackCanary1, canaries[1],
        "The second canary value was overwritten. The stack is corrupted.");
  }

#ifdef FLARE_INTERNAL_USE_ASAN
  static std::pair<void*, std::size_t> SplitMemoryRegionForStack(void* ptr) {
    return {
        reinterpret_cast<char*>(ptr) + fiber::detail::kSystemStackPoisonedSize,
        fiber::detail::kSystemStackSize -
            fiber::detail::kSystemStackPoisonedSize};
  }
#endif
};

}  // namespace flare

namespace flare::fiber::detail {

// Allocate a memory block of size `FLAGS_flare_fiber_stack_size`.
//
// The result points to the top of the stack (lowest address).
inline void* CreateUserStack() { return object_pool::Get<UserStack>().Leak(); }

// `stack` should point to the top of the stack.
inline void FreeUserStack(void* stack) {
  object_pool::Put<UserStack>(reinterpret_cast<UserStack*>(stack));
}

// For allocating / deallocating system stacks.
inline void* CreateSystemStack() {
  return object_pool::Get<SystemStack>().Leak();
}

inline void FreeSystemStack(void* stack) {
  object_pool::Put<SystemStack>(reinterpret_cast<SystemStack*>(stack));
}

}  // namespace flare::fiber::detail

#endif  // FLARE_FIBER_DETAIL_STACK_ALLOCATOR_H_
