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

#include "flare/fiber/shared_mutex.h"

#include <mutex>

namespace flare::fiber {

void SharedMutex::lock() {
  // There can be at most one active writer at a time.
  writer_lock_.lock();  // Unlocked in `unlock()`.
  auto was = reader_quota_.fetch_sub(kMaxReaders, std::memory_order_acquire);
  if (was == kMaxReaders) {
    // No active readers.
    return;
  }
  FLARE_CHECK_LT(was, kMaxReaders);
  FLARE_CHECK_GT(was, 0);

  // Wait until all existing readers (but not the new-comers) finish their job.
  std::unique_lock lk(wakeup_lock_);
  auto pending_readers = kMaxReaders - was;
  wakeup_cv_.wait(lk, [&] { return exited_readers_ == pending_readers; });
  exited_readers_ = 0;  // Reset it for next `lock()`.
  return;
}

bool SharedMutex::try_lock() {
  std::unique_lock lk(writer_lock_, std::try_to_lock);
  if (!lk) {
    return false;
  }
  auto was = reader_quota_.load(std::memory_order_relaxed);
  do {
    if (was != kMaxReaders) {  // Active readers out there.
      return false;
    }
  } while (
      !reader_quota_.compare_exchange_weak(was, 0, std::memory_order_acquire));
  lk.release();  // It's unlocked in `unlock()`.
  return true;
}

void SharedMutex::unlock() {
  std::unique_lock lk(wakeup_lock_);
  auto was = reader_quota_.fetch_add(kMaxReaders, std::memory_order_release);
  FLARE_CHECK_GT(was + kMaxReaders, 0);  // Underflow then.
  // No writer in? It can't be. We're still holding the lock.
  FLARE_CHECK_LE(was, 0);
  if (was != 0) {
    // Unblock all pending readers. (Note that it's possible that a new-comer is
    // "unblocked" by this variable, and starve an old reader. Given that writer
    // should be rare, this shouldn't hurt much.).
    newly_granted_readers_ = -was;
    // Readers waiting.
    wakeup_cv_.notify_all();
  }
  writer_lock_.unlock();  // Allow other writers to come in.
}

void SharedMutex::WaitForRead() {
  std::unique_lock lk(wakeup_lock_);
  wakeup_cv_.wait(lk, [&] {
    if (newly_granted_readers_) {  // Writer has gone.
      --newly_granted_readers_;
      return true;
    }
    return false;
  });
}

void SharedMutex::WakeupWriter() {
  std::scoped_lock _(wakeup_lock_);
  FLARE_CHECK_GE(exited_readers_, 0);
  ++exited_readers_;
  wakeup_cv_.notify_all();
}

}  // namespace flare::fiber
