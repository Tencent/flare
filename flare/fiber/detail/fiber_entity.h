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

#ifndef FLARE_FIBER_DETAIL_FIBER_ENTITY_H_
#define FLARE_FIBER_DETAIL_FIBER_ENTITY_H_

#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>

#include "flare/base/align.h"
#include "flare/base/casting.h"
#include "flare/base/erased_ptr.h"
#include "flare/base/function.h"
#include "flare/base/internal/annotation.h"
#include "flare/base/object_pool.h"
#include "flare/base/ref_ptr.h"
#include "flare/base/thread/spinlock.h"
#include "flare/fiber/detail/fiber_desc.h"
#include "flare/fiber/detail/runnable_entity.h"

namespace flare::fiber::detail {

class ExitBarrier;
class SchedulingGroup;

enum class FiberState { Ready, Running, Waiting, Dead };

// Space reserved at stack bottom for `FiberEntity`.
constexpr auto kFiberStackReservedSize = 512;

// This structure is stored at the top of fiber's stack. (i.e., highest
// address). Everything related to the fiber (control structure) are defined
// here.
//
// Note that `FiberEntity` is stored starting at 64-byte (@sa: kFiberMagicSize)
// offset in last stack page. The beginning `kFiberMagicSize` bytes are used to
// store magic number.
struct alignas(hardware_destructive_interference_size) FiberEntity
    : RunnableEntity {
  using trivial_fls_t = std::aligned_storage_t<8, 8>;

  // This constant determines how many FLS (fiber local storage) are stored in
  // place (inside `FiberEntity`). This helps improving performance in exchange
  // of memory footprint.
  //
  // I haven't test if it suits our need, it's arbitrarily chosen.
  static constexpr auto kInlineLocalStorageSlots = 8;

  // We optimize for FLS of primitive types by storing them inline (without
  // incurring overhead of `ErasedPtr`). This constant controls how many slots
  // are reserved (separately from non-primitive types) for them.
  //
  //   For the moment we only uses trivial FLS in our framework.
  //
  // Only types no larger than `trivial_fls_t` can be stored this way.
  static constexpr auto kInlineTrivialLocalStorageSlots = 8;

  // We don't initialize primitive fields below, they're filled by
  // `InstantiateFiberEntity`. Surprisingly GCC does not eliminate these dead
  // stores (as they're immediately overwritten by `InstantiateFiberEntity`) if
  // we do initialize them here.

  // The first `kFiberMagicSize` bytes serve as a magic for identifying fiber's
  // stack (as well as it's control structure, i.e., us). The magic is:
  // "FLARE_FIBER_ENTITY"
  //
  // It's filled by `CreateUserStack` when the stack is created, we don't want
  // to initialize it each time a fiber is created.
  //
  // char magic[kFiberMagicSize];

  // Fiber ID for `gdb-plugin.py` to use.
  std::uint64_t debugging_fiber_id;

  // This lock is held when the fiber is in state-transition (e.g., from running
  // to suspended). This is required since it's inherent racy when we add
  // ourselves into some wait-chain (and eventually woken up by someone else)
  // and go to sleep. The one who wake us up can be running in a different
  // pthread, and therefore might wake us up even before we actually went sleep.
  // So we always grab this lock before transiting the fiber's state, to ensure
  // that nobody else can change the fiber's state concurrently.
  //
  // For waking up a fiber, this lock is grabbed by whoever the waker;
  // For a fiber to go to sleep, this lock is grabbed by the fiber itself and
  // released by *`SchedulingGroup`* (by the time we're sleeping, we cannot
  // release the lock ourselves.).
  //
  // This lock also protects us from being woken up by several pthread
  // concurrently (in case we waited on several waitables and have not removed
  // us from all of them before more than one of then has fired.).
  Spinlock scheduler_lock;

  // Fiber's affinity to this scheduling group.
  //
  // Set if the fiber should not be stolen to workers that does not belong to
  // the scheduling group specified on fiber's creation.
  bool scheduling_group_local;

  // Set if this fiber was created as a system fiber.
  //
  // System fiber uses a smaller stack, and don't use guard page to detect stack
  // overflow.
  bool system_fiber;

  // Fiber's state.
  FiberState state = FiberState::Ready;

  // Set by `SchedulingGroup::ReadyFiber`.
  //
  // `Waitable`s always schedule fibers that should be woken to the scheduling
  // group specified here.
  SchedulingGroup* scheduling_group;

  // When swapped out, fiber's context is saved here (top of the stack).
  void* state_save_area;

  // Updated when fiber is ready.
  std::uint64_t last_ready_tsc;

  // Set if there is a pending `ResumeOn`. Cleared once `ResumeOn` completes.
  Function<void()> resume_proc;

  // Stack limit. 0 for master fiber.
  std::size_t stack_size;

  // This latch allows you to wait for this fiber's exit. It's needed for
  // implementing `Fiber::join()`.
  //
  // Because we have no idea about which one (`Fiber` or us) will be destroyed
  // first, we share it between `Fiber` and us.
  RefPtr<ExitBarrier> exit_barrier;

  // Fiber local variables stored inline.
  ErasedPtr inline_fls[kInlineLocalStorageSlots];

  // Fiber local variables of primitive types stored inline.
  trivial_fls_t inline_trivial_fls[kInlineTrivialLocalStorageSlots] = {};

  // In case `inline_fls` is not sufficient for storing FLS, `external_fls` is
  // used.
  //
  // However, accessing FLS-es in `external_fls` can be a magnitude slower than
  // `inline_fls`.
  std::unique_ptr<std::unordered_map<std::size_t, ErasedPtr>> external_fls;
  std::unique_ptr<std::unordered_map<std::size_t, trivial_fls_t>>
      external_trivial_fls;

  // Entry point of this fiber. Cleared on first time the fiber is run.
  Function<void()> start_proc;

#ifdef FLARE_INTERNAL_USE_ASAN
  // Lowest address of this fiber's stack.
  const void* asan_stack_bottom = nullptr;

  // Stack limit of this fiber.
  std::size_t asan_stack_size = 0;

  // Set when the fiber is exiting. Special care must be taken in this case
  // (@sa: `SanitizerStartSwitchFiber`).
  bool asan_terminating = false;
#endif  // FLARE_INTERNAL_USE_ASAN

#ifdef FLARE_INTERNAL_USE_TSAN
  // Fiber context used by TSan.
  void* tsan_fiber = nullptr;
#endif  // FLARE_INTERNAL_USE_TSAN

  FiberEntity();

  // Get top (highest address) of the runtime stack (after skipping this
  // control structure).
  //
  // Calling this method on main fiber is undefined.
  void* GetStackTop() const noexcept {
    // The runtime stack is placed right below us.
    return reinterpret_cast<char*>(const_cast<FiberEntity*>(this));
  }

  // Get stack size.
  std::size_t GetStackLimit() const noexcept { return stack_size; }

  // Switch to this fiber.
  void Resume() noexcept;

  // Run code on top this fiber's context (or, if you're experienced with
  // Windows kernel, it's Asynchronous Procedure Call), and then resume this
  // fiber.
  void ResumeOn(Function<void()>&& cb) noexcept;

  // FLS are stored separately for trivial and non-trivial case. Note that you
  // don't have to (and shouldn't) use same index sequence for the two case.

  // Get FLS by its index.
  ErasedPtr* GetFls(std::size_t index) noexcept {
    if (FLARE_LIKELY(index < std::size(inline_fls))) {
      return &inline_fls[index];
    }
    return GetFlsSlow(index);
  }
  ErasedPtr* GetFlsSlow(std::size_t index) noexcept;

  // Get trivial FLS by its index.
  //
  // Trivial FLSes are always zero-initialized.
  trivial_fls_t* GetTrivialFls(std::size_t index) noexcept {
    if (FLARE_LIKELY(index < std::size(inline_trivial_fls))) {
      return &inline_trivial_fls[index];
    }
    return GetTrivialFlsSlow(index);
  }
  trivial_fls_t* GetTrivialFlsSlow(std::size_t index) noexcept;
};

static_assert(sizeof(FiberEntity) < kFiberStackReservedSize);

// The linker should be able to relax these TLS to local-exec when linking
// executables even if we don't specify it explicitly, but it doesn't (in my
// test it only relaxed them to initial-exec). However, specifying initial-exec
// explicitly results in local-exec being used. Thus we specify initial-exec
// here. (Do NOT use local-exec though, it breaks UT, since we're linking UT
// dynamically.).
//
// We use pointer here to avoid call to `__tls_init()` each time it's accessed.
// The "real" master fiber object is defined inside `SetUpMasterFiberEntity()`.
//
// By defining these TLS as `inline` here, compiler can be sure they need no
// special initialization (as the compiler can see their definition, not just
// declaration). Had we defined them in `fiber_entity.cc`, call to "TLS init
// function for ..." might be needed by `GetXxxFiberEntity()` below.
FLARE_INTERNAL_TLS_MODEL inline thread_local FiberEntity* master_fiber;
FLARE_INTERNAL_TLS_MODEL inline thread_local FiberEntity* current_fiber;

// Set up & get master fiber (i.e., so called "main" fiber) of this thread.
void SetUpMasterFiberEntity() noexcept;

// To suppress a (technically correct but in practice) false positive, we can't
// make them inlined. Un-inlining them prevents CSE in accessing TLS (which, in
// TSan's case, is indeed leading to race). It's surprising that CSE only occurs
// when TSan is enabled.
//
// CAUTION: To be pedantic, even in non-TSan environment, this still has to be
// fixed. We actually observed errors caused by TLS accesses here in some
// environment (GCC 10 on ppc64le / aarch64, for instance).
//
// FIXME: We still need a more "standardized" way to address this. I believe
// that the compiler still has the freedom to do CSE even when TSan is not
// enabled. In such case making access current fiber (just as we've done here )
// a function call is too costly.
//
// @sa: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=26461 for more discussions.
#if defined(FLARE_INTERNAL_USE_TSAN) || defined(__powerpc64__) || \
    defined(__aarch64__)

FiberEntity* GetMasterFiberEntity() noexcept;
FiberEntity* GetCurrentFiberEntity() noexcept;
void SetCurrentFiberEntity(FiberEntity* current);

#else

// For the moment we haven't seen any issue with this fast implementation when
// using GCC8.2 on x86-64, so we keep them here for better perf.

// Get & set fiber entity associated with current fiber.
//
// The set method is FOR INTERNAL USE ONLY.
inline FiberEntity* GetCurrentFiberEntity() noexcept { return current_fiber; }

inline void SetCurrentFiberEntity(FiberEntity* current) noexcept {
  current_fiber = current;
}

inline FiberEntity* GetMasterFiberEntity() noexcept { return master_fiber; }

#endif

// Mostly used for debugging purpose.
inline bool IsFiberContextPresent() noexcept {
  return GetCurrentFiberEntity() != nullptr;
}

// Instantiates a fiber entity with information from `desc`. The fiber is
// instantiated so that it would (should) be run in `scheduling_group`.
//
// Ownership of `desc` is taken.
FiberEntity* InstantiateFiberEntity(SchedulingGroup* scheduling_group,
                                    FiberDesc* desc) noexcept;

// Destroys a previously-instantiated fiber entity.
void FreeFiberEntity(FiberEntity* fiber) noexcept;

//////////////////////////////////////////
// Implementation goes below.           //
//////////////////////////////////////////

// Defined in `flare/fiber/detail/{arch}/*.S`
extern "C" void jump_context(void** self, void* to, void* context);

template <class F>
inline void DestructiveRunCallback(F* cb) {
  (*cb)();
  *cb = nullptr;
}

template <class F>
inline void DestructiveRunCallbackOpt(F* cb) {
  if (*cb) {
    (*cb)();
    *cb = nullptr;
  }
}

// This method is used so often that it deserves to be inlined.
inline void FiberEntity::Resume() noexcept {
  // Note that there are some inconsistencies. The stack we're running on is not
  // our stack. This should be easy to see, since we're actually running in
  // caller's context (including its stack).
  auto caller = GetCurrentFiberEntity();
  FLARE_DCHECK_NE(caller, this, "Calling `Resume()` on self is undefined.");

#ifdef FLARE_INTERNAL_USE_ASAN
  // Here we're running on our caller's stack, not ours (the one associated with
  // `*this`.).
  void* shadow_stack;

  // Special care must be taken if the caller is being terminated. In this case,
  // the shadow stack associated with caller must be destroyed. We accomplish
  // this by passing `nullptr` to the call (@sa: `asan::StartSwitchFiber`.).
  flare::internal::asan::StartSwitchFiber(
      caller->asan_terminating ? nullptr : &shadow_stack,  // Caller's shadow
                                                           // stack.
      asan_stack_bottom,  // The stack being swapped in.
      asan_stack_size);   // The stack being swapped in.
#endif

#ifdef FLARE_INTERNAL_USE_TSAN
  flare::internal::tsan::SwitchToFiber(tsan_fiber);
#endif

  // Argument `context` (i.e., `this`) is only used the first time the context
  // is jumped to (in `FiberProc`).
  jump_context(&caller->state_save_area, state_save_area, this);

#ifdef FLARE_INTERNAL_USE_ASAN
  FLARE_CHECK(!caller->asan_terminating);  // Otherwise the caller (as well as
                                           // this runtime stack) has gone.

  // We're back to the caller's runtime stack. Restore its shadow stack.
  flare::internal::asan::CompleteSwitchFiber(shadow_stack);
#endif

  SetCurrentFiberEntity(caller);  // The caller has back.

  // Check for pending `ResumeOn`.
  DestructiveRunCallbackOpt(&caller->resume_proc);
}

}  // namespace flare::fiber::detail

#endif  // FLARE_FIBER_DETAIL_FIBER_ENTITY_H_
