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

#ifndef FLARE_BASE_INTERNAL_BIASED_MUTEX_H_
#define FLARE_BASE_INTERNAL_BIASED_MUTEX_H_

#include <atomic>
#include <mutex>

#include "flare/base/internal/annotation.h"
#include "flare/base/internal/memory_barrier.h"
#include "flare/base/likely.h"

namespace flare::internal {

namespace detail::biased_mutex {

// Helper class for implementing fast-path of `BiasedMutex.`
template <class Owner>
class BlessedSide {
 public:
  void lock();
  void unlock();

 private:
  void LockSlow();
  Owner* GetOwner() noexcept { return static_cast<Owner*>(this); }
};

// Helper class for implementing slow-path of `BiasedMutex.`
template <class Owner>
class ReallySlowSide {
 public:
  void lock();
  void unlock();

 private:
  Owner* GetOwner() noexcept { return static_cast<Owner*>(this); }
};

}  // namespace detail::biased_mutex

// TL;DR: DO NOT USE IT. IT'S TERRIBLY SLOW.
//
// This mutex is "biased" in that it boosts one ("blessed") side's perf. in
// grabbing lock, by sacrificing other contenders. This mutex can boost overall
// perf. if you're using it in scenarios where you have separate fast-path and
// slow-path (which should be executed rare). Note that there can only be one
// "blessed" side. THE SLOW SIDE IS **REALLY REALLY** SLOW AND MAY HAVE A
// NEGATIVE IMPACT ON OTHER THREADS (esp. considering that the heavy side of our
// asymmetric memory barrier performs really bad). Incorrect use of this mutex
// can hurt performane badly. YOU HAVE BEEN WARNED.
//
// Note that it's a SPINLOCK. In case your critical section is long, do not use
// it.
//
// Internally it's a "Dekker's lock". By using asymmetric memory barrier (@sa:
// `memory_barrier.h`), we can eliminate both RMW atomic & "actual" memory
// barrier in fast path.
//
// @sa: https://en.wikipedia.org/wiki/Dekker%27s_algorithm
//
// Usage:
//
// // Fast path.
// std::scoped_lock _(*biased_mutex.blessed_side());
//
// // Slow path.
// std::scoped_lock _(*biased_mutex.really_slow_side());
class BiasedMutex : private detail::biased_mutex::BlessedSide<BiasedMutex>,
                    private detail::biased_mutex::ReallySlowSide<BiasedMutex> {
  using BlessedSide = detail::biased_mutex::BlessedSide<BiasedMutex>;
  using ReallySlowSide = detail::biased_mutex::ReallySlowSide<BiasedMutex>;
  friend class detail::biased_mutex::BlessedSide<BiasedMutex>;
  friend class detail::biased_mutex::ReallySlowSide<BiasedMutex>;

 public:
#ifdef FLARE_INTERNAL_USE_TSAN
  BiasedMutex() {
    FLARE_INTERNAL_TSAN_MUTEX_CREATE(this, __tsan_mutex_not_static);
  }

  ~BiasedMutex() { FLARE_INTERNAL_TSAN_MUTEX_DESTROY(this, 0); }
#endif

  BlessedSide* blessed_side() { return this; }
  ReallySlowSide* really_slow_side() { return this; }

 private:
  std::atomic<bool> wants_to_enter_[2] = {};
  std::atomic<std::uint8_t> turn_ = 0;

  // (Our implementation of) Dekker's lock only allows two participants, so we
  // use this lock to serialize contenders in slow path.
  std::mutex slow_lock_lock_;
};

//////////////////////////////////////
// Implementation goes below.       //
//////////////////////////////////////

namespace detail::biased_mutex {

template <class Owner>
inline void BlessedSide<Owner>::lock() {
  FLARE_INTERNAL_TSAN_MUTEX_PRE_LOCK(this, 0);

  GetOwner()->wants_to_enter_[0].store(true, std::memory_order_relaxed);
  AsymmetricBarrierLight();
  // There's no need to synchronizes with "other" bless-side -- There won't be
  // one. This lock permits only one bless-side, i.e., us.
  //
  // Therefore we only have to synchronize with the slow side. This is achieved
  // by acquiring on `wants_to_enter_[1]`.
  if (FLARE_UNLIKELY(
          GetOwner()->wants_to_enter_[1].load(std::memory_order_acquire))) {
    LockSlow();
  }

  FLARE_INTERNAL_TSAN_MUTEX_POST_LOCK(this, 0, 0);
}

template <class Owner>
[[gnu::noinline]] void BlessedSide<Owner>::LockSlow() {
  AsymmetricBarrierLight();  // Not necessary, TBH.
  while (FLARE_UNLIKELY(
      // Synchronizes with the slow side.
      GetOwner()->wants_to_enter_[1].load(std::memory_order_acquire))) {
    if (GetOwner()->turn_.load(std::memory_order_relaxed) != 0) {
      GetOwner()->wants_to_enter_[0].store(false, std::memory_order_relaxed);
      while (GetOwner()->turn_.load(std::memory_order_relaxed) != 0) {
        // Spin.
      }
      GetOwner()->wants_to_enter_[0].store(true, std::memory_order_relaxed);
      AsymmetricBarrierLight();
    }
  }
}

template <class Owner>
inline void BlessedSide<Owner>::unlock() {
  FLARE_INTERNAL_TSAN_MUTEX_PRE_UNLOCK(this, 0);

  GetOwner()->turn_.store(1, std::memory_order_relaxed);
  // Synchronizes with the slow side.
  GetOwner()->wants_to_enter_[0].store(false, std::memory_order_release);

  FLARE_INTERNAL_TSAN_MUTEX_POST_UNLOCK(this, 0);
}

template <class Owner>
void ReallySlowSide<Owner>::lock() {
  FLARE_INTERNAL_TSAN_MUTEX_PRE_LOCK(this, 0);

  GetOwner()->slow_lock_lock_.lock();
  GetOwner()->wants_to_enter_[1].store(true, std::memory_order_relaxed);
  AsymmetricBarrierHeavy();
  while (FLARE_UNLIKELY(
      // Synchronizes with the fast side.
      GetOwner()->wants_to_enter_[0].load(std::memory_order_acquire))) {
    if (GetOwner()->turn_.load(std::memory_order_relaxed) != 1) {
      GetOwner()->wants_to_enter_[1].store(false, std::memory_order_relaxed);
      while (GetOwner()->turn_.load(std::memory_order_relaxed) != 1) {
        // Spin.
      }
      GetOwner()->wants_to_enter_[1].store(true, std::memory_order_relaxed);
      AsymmetricBarrierHeavy();
    }
  }

  FLARE_INTERNAL_TSAN_MUTEX_POST_LOCK(this, 0, 0);
}

template <class Owner>
void ReallySlowSide<Owner>::unlock() {
  FLARE_INTERNAL_TSAN_MUTEX_PRE_UNLOCK(this, 0);

  GetOwner()->turn_.store(0, std::memory_order_relaxed);
  // Synchronizes with the fast side.
  GetOwner()->wants_to_enter_[1].store(false, std::memory_order_release);
  GetOwner()->slow_lock_lock_.unlock();

  FLARE_INTERNAL_TSAN_MUTEX_POST_UNLOCK(this, 0);
}

};  // namespace detail::biased_mutex

}  // namespace flare::internal

#endif  // FLARE_BASE_INTERNAL_BIASED_MUTEX_H_
