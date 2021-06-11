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

#include "flare/fiber/detail/fiber_entity.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1  // `pthread_getattr_np` needs this.
#endif

#include <pthread.h>

#include <limits>
#include <utility>

#include "flare/base/deferred.h"
#include "flare/base/id_alloc.h"
#include "flare/base/internal/annotation.h"
#include "flare/base/logging.h"
#include "flare/base/string.h"
#include "flare/fiber/detail/scheduling_group.h"
#include "flare/fiber/detail/stack_allocator.h"
#include "flare/fiber/detail/waitable.h"
#include "gflags/gflags.h"

DECLARE_int32(flare_fiber_stack_size);

// Defined in `flare/fiber/detail/{arch}/*.S`
extern "C" {

void* make_context(void* sp, std::size_t size, void (*start_proc)(void*));
}

namespace flare::fiber::detail {

namespace {

struct FiberIdTraits {
  using Type = std::uint64_t;
  static constexpr auto kMin = 1;
  static constexpr auto kMax = std::numeric_limits<std::uint64_t>::max();
  // I don't expect a pthread worker need to create more than 128K fibers per
  // sec.
  static constexpr auto kBatchSize = 131072;
};

#ifdef FLARE_INTERNAL_USE_ASAN

// Returns: stack bottom (lowest address) and limit of current pthread (i.e.
// master fiber's stack).
std::pair<const void*, std::size_t> GetMasterFiberStack() noexcept {
  pthread_attr_t self_attr;
  FLARE_PCHECK(pthread_getattr_np(pthread_self(), &self_attr) == 0);
  ScopedDeferred _(
      [&] { FLARE_PCHECK(pthread_attr_destroy(&self_attr) == 0); });

  void* stack;
  std::size_t limit;
  FLARE_PCHECK(pthread_attr_getstack(&self_attr, &stack, &limit) == 0);
  return {stack, limit};
}

#endif

}  // namespace

// Entry point for newly-started fibers.
//
// NOT put into anonymous namespace to simplify its displayed name in GDB.
//
// `extern "C"` doesn't help, unfortunately. (It does simplify mangled name,
// though.)
//
// Do NOT mark this function as `noexcept`. We don't want to force stack being
// unwound on exception.
static void FiberProc(void* context)  {
  auto self = reinterpret_cast<FiberEntity*>(context);
  // We're running in `self`'s stack now.

#ifdef FLARE_INTERNAL_USE_ASAN
  // A new fiber has born, so complete with a new shadow stack.
  //
  // By passing `nullptr` to this call, a new shadow stack is allocated
  // internally. (@sa: `asan::CompleteSwitchFiber`).
  flare::internal::asan::CompleteSwitchFiber(nullptr);
#endif

  SetCurrentFiberEntity(self);  // We're alive.
  self->state = FiberState::Running;
  self->ever_started_magic = kFiberEverStartedMagic;

  // Hmmm, there is a pending resumption callback, even if we haven't completely
  // started..
  //
  // We'll run it anyway. This, for now, is mostly used for `Dispatch` fiber
  // launch policy.
  DestructiveRunCallbackOpt(&self->resume_proc);
  DestructiveRunCallback(&self->start_proc);

  // We're leaving now.
  FLARE_CHECK_EQ(self, GetCurrentFiberEntity());

  // This fiber should not be waiting on anything (mutex / condition_variable
  // / ...), i.e., no one else should be referring this fiber (referring to its
  // `exit_barrier` is, since it's ref-counted, no problem), otherwise it's a
  // programming mistake.

  // Let's see if there will be someone who will be waiting on us.
  if (!self->exit_barrier) {
    // Mark the fiber as dead. This prevent our GDB plugin from listing this
    // fiber out.
    self->state = FiberState::Dead;

#ifdef FLARE_INTERNAL_USE_ASAN
    // We're leaving, a special call to asan is required. So take special note
    // of it.
    //
    // Consumed by `FiberEntity::Resume()` prior to switching stack.
    self->asan_terminating = true;
#endif

    // No one is waiting for us, this is easy.
    GetMasterFiberEntity()->ResumeOn([self] { FreeFiberEntity(self); });
  } else {
    // The lock must be taken first, we can't afford to block when we (the
    // callback passed to `ResumeOn()`) run on master fiber.
    //
    // CAUTION: WE CAN TRIGGER RESCHEDULING HERE.
    auto ebl = self->exit_barrier->GrabLock();

    // Must be done after `GrabLock()`, as `GrabLock()` is self may trigger
    // rescheduling.
    self->state = FiberState::Dead;

#ifdef FLARE_INTERNAL_USE_ASAN
    self->asan_terminating = true;
#endif

    // We need to switch to master fiber and free the resources there, there's
    // no call-stack for us to return.
    GetMasterFiberEntity()->ResumeOn([self, lk = std::move(ebl)]() mutable {
      // The `exit_barrier` is move out so as to free `self` (the stack)
      // earlier. Stack resource is precious.
      auto eb = std::move(self->exit_barrier);

      // Because no one else if referring `self` (see comments above), we're
      // safe to free it here.
      FreeFiberEntity(self);  // Good-bye.

      // Were anyone waiting on us, wake them up now.
      eb->UnsafeCountDown(std::move(lk));
    });
  }
  FLARE_CHECK(0);  // Can't be here.
}

void FiberEntity::ResumeOn(Function<void()>&& cb) noexcept {
  auto caller = GetCurrentFiberEntity();
  FLARE_CHECK(!resume_proc,
              "You may not call `ResumeOn` on a fiber twice (before the first "
              "one has executed).");
  FLARE_CHECK_NE(caller, this, "Calling `Resume()` on self is undefined.");

  // This pending call will be performed and cleared immediately when we
  // switched to `*this` fiber (before calling user's continuation).
  resume_proc = std::move(cb);
  Resume();
}

ErasedPtr* FiberEntity::GetFlsSlow(std::size_t index) noexcept {
  FLARE_LOG_WARNING_ONCE(
      "Excessive FLS usage. Performance will likely degrade.");
  if (FLARE_UNLIKELY(!external_fls)) {
    external_fls =
        std::make_unique<std::unordered_map<std::size_t, ErasedPtr>>();
  }
  return &(*external_fls)[index];
}

FiberEntity::trivial_fls_t* FiberEntity::GetTrivialFlsSlow(
    std::size_t index) noexcept {
  FLARE_LOG_WARNING_ONCE(
      "Excessive FLS usage. Performance will likely degrade.");
  if (FLARE_UNLIKELY(!external_trivial_fls)) {
    external_trivial_fls =
        std::make_unique<std::unordered_map<std::size_t, trivial_fls_t>>();
  }
  return &(*external_trivial_fls)[index];
}

void SetUpMasterFiberEntity() noexcept {
  thread_local FiberEntity master_fiber_impl;

  master_fiber_impl.debugging_fiber_id = -1;
  master_fiber_impl.state_save_area = nullptr;
  master_fiber_impl.state = FiberState::Running;
  master_fiber_impl.stack_size = 0;

  master_fiber_impl.scheduling_group = SchedulingGroup::Current();

#ifdef FLARE_INTERNAL_USE_ASAN
  std::tie(master_fiber_impl.asan_stack_bottom,
           master_fiber_impl.asan_stack_size) = GetMasterFiberStack();
#endif

#ifdef FLARE_INTERNAL_USE_TSAN
  master_fiber_impl.tsan_fiber = flare::internal::tsan::GetCurrentFiber();
#endif

  master_fiber = &master_fiber_impl;
  SetCurrentFiberEntity(master_fiber);
}

#if defined(FLARE_INTERNAL_USE_TSAN) || defined(__powerpc64__) || \
    defined(__aarch64__)

FiberEntity* GetMasterFiberEntity() noexcept { return master_fiber; }

FiberEntity* GetCurrentFiberEntity() noexcept { return current_fiber; }

void SetCurrentFiberEntity(FiberEntity* current) { current_fiber = current; }

#endif

FiberEntity* CreateFiberEntity(SchedulingGroup* sg, bool system_fiber,
                               Function<void()>&& start_proc) noexcept {
  auto stack = system_fiber ? CreateSystemStack() : CreateUserStack();
  auto stack_size =
      system_fiber ? kSystemStackSize : FLAGS_flare_fiber_stack_size;
  auto bottom = reinterpret_cast<char*>(stack) + stack_size;
  // `FiberEntity` (and magic) is stored at the stack bottom.
  auto ptr = bottom - kFiberStackReservedSize;
  FLARE_DCHECK(reinterpret_cast<std::uintptr_t>(ptr) % alignof(FiberEntity) ==
               0);
  // NOT value-initialized intentionally, to save precious CPU cycles.
  auto fiber = new (ptr) FiberEntity;  // A new life has born.

  fiber->debugging_fiber_id = id_alloc::Next<FiberIdTraits>();
  // `fiber->ever_started_magic` is not filled here. @sa: `FiberProc`.
  fiber->system_fiber = system_fiber;
  fiber->stack_size = stack_size - kFiberStackReservedSize;
  fiber->state_save_area =
      make_context(fiber->GetStackTop(), fiber->GetStackLimit(), FiberProc);
  fiber->scheduling_group = sg;
  fiber->start_proc = std::move(start_proc);
  fiber->state = FiberState::Ready;

#ifdef FLARE_INTERNAL_USE_ASAN
  // Using the lowest VA here is NOT a mistake.
  //
  // Despite the fact that we normally treat the bottom of the stack as the
  // highest VA possible (assuming the stack grows down), AFAICT from source
  // code of ASan, it does expect the lowest VA here.
  //
  // @sa: `gcc/libsanitizer/asan/asan_thread.cpp`:
  //
  // > 117 void AsanThread::StartSwitchFiber(...) {
  // ...
  // > 124   next_stack_bottom_ = bottom;
  // > 125   next_stack_top_ = bottom + size;
  // > 126   atomic_store(&stack_switching_, 1, memory_order_release);
  fiber->asan_stack_bottom = stack;

  // NOT `fiber->GetStackLimit()`.
  //
  // Reserved space is also made accessible to ASan. These bytes might be
  // accessed by the fiber later (e.g., `start_proc`.).
  fiber->asan_stack_size = stack_size;
#endif

#ifdef FLARE_INTERNAL_USE_TSAN
  fiber->tsan_fiber = flare::internal::tsan::CreateFiber();
#endif

  return fiber;
}

void FreeFiberEntity(FiberEntity* fiber) noexcept {
  bool system_fiber = fiber->system_fiber;

#ifdef FLARE_INTERNAL_USE_TSAN
  flare::internal::tsan::DestroyFiber(fiber->tsan_fiber);
#endif

  fiber->ever_started_magic = 0;  // Hopefully the compiler does not optimize
                                  // this away.
  fiber->~FiberEntity();

  auto p = reinterpret_cast<char*>(fiber) + kFiberStackReservedSize -
           (system_fiber ? kSystemStackSize : FLAGS_flare_fiber_stack_size);
  if (system_fiber) {
    FreeSystemStack(p);
  } else {
    FreeUserStack(p);
  }
}

}  // namespace flare::fiber::detail
