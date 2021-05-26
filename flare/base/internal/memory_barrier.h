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

#ifndef FLARE_BASE_INTERNAL_MEMORY_BARRIER_H_
#define FLARE_BASE_INTERNAL_MEMORY_BARRIER_H_

#include <atomic>

namespace flare::internal {

// Compiler barrier.
//
// This type of barrier has nothing to do with actual CPU reordering, it only
// prevents the compiler from reordering things.
inline void CompilerBarrier() {
#ifdef __x86_64__
  asm volatile("" ::: "memory");
#else
  // Not sure if it's intended to be used this way.
  std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}

// Prevents reorder between reads.
inline void ReadBarrier() {
#ifdef __x86_64__
  asm volatile("" ::: "memory");
#else
  // Perhaps too heavy for some ISAs. TODO(luobogao): Refine it.
  std::atomic_thread_fence(std::memory_order_seq_cst);
#endif
}

// Prevents reorder between writes.
inline void WriteBarrier() {
  // Perhaps too heavy for some ISAs. TODO(luobogao): Refine it.
  std::atomic_thread_fence(std::memory_order_seq_cst);
}

// On x86-64, `lock xxx` should provide the same fence semantics (except for
// ordering regarding non-temporal operations) as `mfence`, while being much
// faster (~8ns vs ~20ns).
//
// @sa: https://stackoverflow.com/a/50279772
//
// Besides, `mfence` on certain ISAs (e.g., SKL) is a serializing instruction
// (like `rdtscp` / `lfence`, etc.), this introduces unnecessary overhead.
//
// @sa: https://stackoverflow.com/a/50496379
inline void MemoryBarrier() {
#ifdef __x86_64__
  // Implies full barrier on x86-64.
  //
  // @sa: https://lore.kernel.org/patchwork/patch/850075/
  // @sa: https://shipilev.net/blog/2014/on-the-fence-with-dependencies/
  //
  // `-32(%rsp)` (32 is chosen arbitrarily) can overflow if the thread is
  // already approaching the stack bottom. But in this case it will likely
  // overflow the stack anyway.. Not using `0(%rsp)` avoids introducing possible
  // false dependency. (128 can work, but I suspect that would incur unnecessary
  // cache miss.)
  asm volatile("lock; addl $0,-32(%%rsp)" ::: "memory", "cc");
#else
  // `std::atomic_thread_fence` is not meant to be used this way..
  std::atomic_thread_fence(std::memory_order_seq_cst);
#endif
}

// Asymmetric memory barrier allows you to boost one side of barrier issuer in
// trade of another side. In situations where the two sides are executed
// unequally frequently, this can boost overall performance.
//
// Note that:
//
// - Lightweight barrier must be paired with a heavy one. There's no ordering
//   guarantee between two lightweight barriers (as NO code is actually
//   generated.).
//
//   Memory barriers must be paired in general, whether a asymmetric one or not.
//
// - It's not always make sense to use asymmetric barrier unless the slow side
//   is indeed hardly executed. Excessive calls to heavy barrier can degrade
//   performance significantly.
//
// @sa: https://lwn.net/Articles/640239/
// @sa: http://man7.org/linux/man-pages/man2/membarrier.2.html

// The "blessed" side of barrier. This is the faster side (no code is actually
// generated).
inline void AsymmetricBarrierLight() { CompilerBarrier(); }

// The slower side of asymmetric memory barrier.
//
// CAUTION: For the moment it's implemented via `mmap`. This implementation is
// **EXTREMELY** SLOW. Besides, issueing this barrier CAN HAVE A NEGATIVE IMPACT
// ON OTHER THREADS.
void AsymmetricBarrierHeavy();

}  // namespace flare::internal

#endif  // FLARE_BASE_INTERNAL_MEMORY_BARRIER_H_
