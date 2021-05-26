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

#ifndef FLARE_BASE_INTERNAL_ANNOTATION_H_
#define FLARE_BASE_INTERNAL_ANNOTATION_H_

#include <cstddef>

#include "flare/base/internal/logging.h"

#ifdef __clang__
#if __has_feature(address_sanitizer)
#define FLARE_INTERNAL_USE_ASAN 1
#endif  // __has_feature(address_sanitizer)

#if __has_feature(thread_sanitizer)
#define FLARE_INTERNAL_USE_TSAN 1
#endif  // __has_feature(thread_sanitizer)

#define FLARE_INTERNAL_NO_SANITIZE_ADDRESS [[clang::no_sanitize("address")]]
#define FLARE_INTERNAL_NO_SANITIZE_THREAD [[clang::no_sanitize("thread")]]
#endif  // __clang__

#if defined(__GNUC__) && !defined(__clang__)
#ifdef __SANITIZE_ADDRESS__  // GCC
#define FLARE_INTERNAL_USE_ASAN 1
#endif  // __SANITIZE_ADDRESS__

#ifdef __SANITIZE_THREAD__
#define FLARE_INTERNAL_USE_TSAN 1
#endif  // __SANITIZE_THREAD__

#define FLARE_INTERNAL_NO_SANITIZE_ADDRESS [[gnu::no_sanitize("address")]]
#define FLARE_INTERNAL_NO_SANITIZE_THREAD [[gnu::no_sanitize("thread")]]

#endif  // defined(__GNUC__) && !defined(__clang__)

#ifdef FLARE_INTERNAL_USE_ASAN
extern "C" {
[[gnu::weak]] void __asan_poison_memory_region(void const volatile*, size_t);
[[gnu::weak]] void __asan_unpoison_memory_region(void const volatile*, size_t);
[[gnu::weak]] void __sanitizer_start_switch_fiber(void**, const void*, size_t);
[[gnu::weak]] void __sanitizer_finish_switch_fiber(void*, const void**,
                                                   size_t*);
}
#endif  // FLARE_INTERNAL_USE_ASAN

// FIXME: TSan support does not work yet.
#ifdef FLARE_INTERNAL_USE_TSAN
extern "C" {
[[gnu::weak]] void* __tsan_get_current_fiber(void);
[[gnu::weak]] void* __tsan_create_fiber(unsigned);
[[gnu::weak]] void __tsan_destroy_fiber(void*);
[[gnu::weak]] void __tsan_switch_to_fiber(void*, unsigned);
[[gnu::weak]] void __tsan_set_fiber_name(void*, const char*);
const unsigned __tsan_switch_to_fiber_no_sync = 1 << 0;

// No special wrapper for these annotations (unlike annotations for fiber) as
// they're widely supported.
[[gnu::weak]] void __tsan_mutex_create(void*, unsigned);
[[gnu::weak]] void __tsan_mutex_destroy(void*, unsigned);
[[gnu::weak]] void __tsan_mutex_pre_lock(void*, unsigned);
[[gnu::weak]] void __tsan_mutex_post_lock(void*, unsigned, int);
[[gnu::weak]] int __tsan_mutex_pre_unlock(void*, unsigned);
[[gnu::weak]] void __tsan_mutex_post_unlock(void*, unsigned);
const unsigned __tsan_mutex_linker_init = 1 << 0;
const unsigned __tsan_mutex_write_reentrant = 1 << 1;
const unsigned __tsan_mutex_read_reentrant = 1 << 2;
const unsigned __tsan_mutex_not_static = 1 << 8;
const unsigned __tsan_mutex_read_lock = 1 << 3;
const unsigned __tsan_mutex_try_lock = 1 << 4;
const unsigned __tsan_mutex_try_lock_failed = 1 << 5;
const unsigned __tsan_mutex_recursive_lock = 1 << 6;
const unsigned __tsan_mutex_recursive_unlock = 1 << 7;

// Helper for establishing happens-before relationship.
[[gnu::weak]] void __tsan_acquire(void*);
[[gnu::weak]] void __tsan_release(void*);
}

// TODO(luobogao): Annotations for fiber, to replace our own implementation.

#define FLARE_INTERNAL_TSAN_MUTEX_CREATE(...) __tsan_mutex_create(__VA_ARGS__)
#define FLARE_INTERNAL_TSAN_MUTEX_DESTROY(...) __tsan_mutex_destroy(__VA_ARGS__)
#define FLARE_INTERNAL_TSAN_MUTEX_PRE_LOCK(...) \
  __tsan_mutex_pre_lock(__VA_ARGS__)
#define FLARE_INTERNAL_TSAN_MUTEX_POST_LOCK(...) \
  __tsan_mutex_post_lock(__VA_ARGS__)
#define FLARE_INTERNAL_TSAN_MUTEX_PRE_UNLOCK(...) \
  __tsan_mutex_pre_unlock(__VA_ARGS__)
#define FLARE_INTERNAL_TSAN_MUTEX_POST_UNLOCK(...) \
  __tsan_mutex_post_unlock(__VA_ARGS__)

#else

#define FLARE_INTERNAL_TSAN_MUTEX_CREATE(...) (void)0
#define FLARE_INTERNAL_TSAN_MUTEX_DESTROY(...) (void)0
#define FLARE_INTERNAL_TSAN_MUTEX_PRE_LOCK(...) (void)0
#define FLARE_INTERNAL_TSAN_MUTEX_POST_LOCK(...) (void)0
#define FLARE_INTERNAL_TSAN_MUTEX_PRE_UNLOCK(...) (void)0
#define FLARE_INTERNAL_TSAN_MUTEX_POST_UNLOCK(...) (void)0

#endif  // FLARE_INTERNAL_USE_TSAN

// Using `initial-exec` prevents binary from being loaded via `dlopen`. Flare is
// not intended to be used that way (via `dlopen`). Yet in case there's such
// need, you can define `FLARE_USE_SLOW_TLS_MODEL` to accomplish that.
//
// @sa: `doc/performance-guide.md` for more details.
#if !defined(FLARE_USE_SLOW_TLS_MODEL)
#define FLARE_INTERNAL_TLS_MODEL [[gnu::tls_model("initial-exec")]]
#else
#define FLARE_INTERNAL_TLS_MODEL /* Nothing, using the default model. */
#endif

namespace flare::internal {

#ifdef FLARE_INTERNAL_USE_ASAN

namespace asan {

// Check if `fp` is supported by the runtime.
//
// Well, to be pedantic, the error message is imprecise. It's the version of
// runtime, not the compiler, matters.
//
// But why would someone use a different version of runtime than the compiler
// anyway? Noticing the user about the compiler version would be easier for he /
// she to fix (as some user might not have a notion of runtime's version).
#define FLARE_INTERNAL_ASAN_CHECK(fp) \
  FLARE_CHECK(fp, "Your compiler is too old to use ASan with fibers.");

// You need to call this method before swapping runtime stack.
//
// Internally ASan keeps a shadow stack (called "fake stack" officially) to
// trace stack usage. On stack swap, this method releases (read, "leak") the
// shadow stack's ownership to you via `shadow_stack` for later restoration.
// When you swap **this** runtime stack back, you should call
// `CompleteSwitchFiber` with this shadow stack to restore it.
//
// To destroy a shadow stack, call this method with `shadow_stack` set to
// `nullptr`.
//
// `stack_bottom` & `stack_limit` specifies the new (the one you're swapping in)
// stack's bottom (lowest address) and its limit, respectively.
inline void StartSwitchFiber(void** shadow_stack, const void* stack_bottom,
                             std::size_t stack_limit) {
  FLARE_INTERNAL_ASAN_CHECK(__sanitizer_start_switch_fiber);
  __sanitizer_start_switch_fiber(shadow_stack, stack_bottom, stack_limit);
}

// When you complete stack switch, you need to call this method.
//
// `shadow_stack` should be the one given to you when you call
// `StartSwitchFiber`, i.e., **this** (the one you're currently, and will be,
// running on until next stack swap) stack's shadow. By calling this method, the
// shadow stack's ownership is given back to the sanitizer.
//
// For a new stack (that hasn't been in use before), you should call this method
// with `nullptr`. In this case, a new shadow stack is allocated by the
// sanitizer.
inline void CompleteSwitchFiber(void* shadow_stack) {
  FLARE_INTERNAL_ASAN_CHECK(__sanitizer_finish_switch_fiber);
  __sanitizer_finish_switch_fiber(shadow_stack, nullptr, nullptr);
}

// "Poison" or "Un-poison" a memory region.
//
// ASan internally "poison"s memory addresses that it deems "invalid" (e.g.,
// when variable on stack goes out of scope.). This helps recognize cases such
// as use-after-free. However, in case you're recycling memory segments (e.g.,
// by pooling fiber stacks) or allocate / deallocate via syscall (which ASan is
// oblivious), you're out of luck.
//
// Simply put, to reuse memories, you need "un-poison" them before using them.
inline void PoisonMemoryRegion(const void* ptr, std::size_t size) {
  FLARE_INTERNAL_ASAN_CHECK(__asan_poison_memory_region);
  __asan_poison_memory_region(ptr, size);
}

inline void UnpoisonMemoryRegion(const void* ptr, std::size_t size) {
  FLARE_INTERNAL_ASAN_CHECK(__asan_unpoison_memory_region);
  __asan_unpoison_memory_region(ptr, size);
}

#undef FLARE_INTERNAL_ASAN_CHECK

}  // namespace asan

#else

// Left undefined otherwise.

#endif  // FLARE_INTERNAL_USE_ASAN

#ifdef FLARE_INTERNAL_USE_TSAN

namespace tsan {

// Check if `fp` is supported by the runtime.
//
// GCC 10 (or higher) / Clang 9 (or higher) is required for using TSan with
// fibers.
//
// For users: If you go here to check why your program crashes, you're likely
// out of luck. The compiler version requirement is relatively high, admittedly.
#define FLARE_INTERNAL_TSAN_CHECK(fp) \
  FLARE_CHECK(fp, "Your compiler is too old to use TSan with fibers.");

// Internally TSan maintains a context (a shadow stack, actually) for each
// thread. In case of fibers, the context is required for each fiber.
//
// To use TSan with fibers, you need to notify TSan about them.

// Create a new TSan fiber context.
inline void* CreateFiber() {
  FLARE_INTERNAL_TSAN_CHECK(__tsan_create_fiber);
  return __tsan_create_fiber(0);
}

// Destroy a TSan fiber context.
inline void DestroyFiber(void* fiber) {
  FLARE_INTERNAL_TSAN_CHECK(__tsan_destroy_fiber);
  __tsan_destroy_fiber(fiber);
}

// Switch fiber context in TSan.
inline void SwitchToFiber(void* fiber) {
  FLARE_INTERNAL_TSAN_CHECK(__tsan_switch_to_fiber);
  // Do NOT specify `__tsan_switch_to_fiber_no_sync` here, otherwise every
  // access to TLS results in a false positive.
  __tsan_switch_to_fiber(fiber, 0);
}

// Get current fiber context in this thread. If no fiber were switched to, the
// master fiber's (i.e., pthread's) context is returned.
//
// Normally you would need this to set up your master fiber.
inline void* GetCurrentFiber() {
  FLARE_INTERNAL_TSAN_CHECK(__tsan_get_current_fiber);
  return __tsan_get_current_fiber();
}

// Setting a name for fiber makes your failure experience better. (It's still a
// failure if you see the name, though.)
inline void SetFiberName(void* fiber, const char* name) {
  FLARE_INTERNAL_TSAN_CHECK(__tsan_set_fiber_name);
  __tsan_set_fiber_name(fiber, name);
}

#undef FLARE_INTERNAL_TSAN_CHECK

}  // namespace tsan

#else

// Left undefined otherwise.

#endif  // FLARE_INTERNAL_USE_TSAN

}  // namespace flare::internal

#endif  // FLARE_BASE_INTERNAL_ANNOTATION_H_
