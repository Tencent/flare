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

#ifndef FLARE_FIBER_SHARED_MUTEX_H_
#define FLARE_FIBER_SHARED_MUTEX_H_

#include <atomic>
#include <cinttypes>

#include "flare/base/likely.h"
#include "flare/fiber/condition_variable.h"
#include "flare/fiber/mutex.h"

namespace flare::fiber {

// Implements `std::shared_mutex` alternative for fiber.
//
// CAUTION: Performance-wise, reader-writer lock does NOT perform well unless
// your critical section is sufficient large. In certain cases, reader-writer
// lock can perform worse than `Mutex`. If reader performance is critical to
// you, consider using other methods (e.g., thread-local cache, hazard pointers,
// ...).
//
// The implementation is inspired by (but not exactly the same):
// https://eli.thegreenplace.net/2019/implementing-reader-writer-locks/
class SharedMutex {
 public:
  // Lock / unlock in exclusive mode (writer-side).
  //
  // Implementation of write side is slow.
  void lock();
  bool try_lock();
  void unlock();

  // Lock / unlock in shared mode (reader-side).
  //
  // Optimized for non-contending (with writer) case.
  void lock_shared();
  bool try_lock_shared();
  void unlock_shared();

 private:
  void WaitForRead();
  void WakeupWriter();

 private:
  inline static constexpr auto kMaxReaders = 0x3fff'ffff;

  // Positive if no writer pending. Negative if (exactly) one writer is waiting.
  std::atomic<std::int32_t> reader_quota_{kMaxReaders};

  // Synchronizes readers and writers.
  fiber::Mutex wakeup_lock_;  // Acquired after `writer_lock_` if both acquired.
  fiber::ConditionVariable wakeup_cv_;
  int exited_readers_ = 0;
  int newly_granted_readers_ = 0;

  // Resolves contention between writers. This guarantees us that no more than
  // one writer can wait on `readers_`.
  fiber::Mutex writer_lock_;
};

////////////////////////////////////////
// Implementation goes below.         //
////////////////////////////////////////

inline void SharedMutex::lock_shared() {
  if (auto was = reader_quota_.fetch_sub(1, std::memory_order_acquire);
      FLARE_LIKELY(was > 1)) {
    // Reader lock grabbed.
    FLARE_CHECK_LE(was, kMaxReaders);
  } else {
    FLARE_CHECK_NE(was, 1);  // Underflow then.
    return WaitForRead();
  }
}

inline bool SharedMutex::try_lock_shared() {
  auto was = reader_quota_.load(std::memory_order_relaxed);
  do {
    FLARE_CHECK_LE(was, kMaxReaders);
    FLARE_CHECK_NE(was, 1);  // Underflow then.
    if (was <= 0) {
      return false;
    }
  } while (!reader_quota_.compare_exchange_weak(was, was - 1,
                                                std::memory_order_acquire));
  return true;
}

inline void SharedMutex::unlock_shared() {
  if (auto was = reader_quota_.fetch_add(1, std::memory_order_release);
      FLARE_LIKELY(was > 0)) {
    // No writer is waiting, nothing to do.
    FLARE_CHECK_LT(was, kMaxReaders);
  } else {
    return WakeupWriter();
  }
}

}  // namespace flare::fiber

#endif  // FLARE_FIBER_SHARED_MUTEX_H_
