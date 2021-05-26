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

#ifndef FLARE_BASE_THREAD_LATCH_H_
#define FLARE_BASE_THREAD_LATCH_H_

#include <condition_variable>
#include <mutex>

#include "flare/base/internal/logging.h"

namespace flare {

// @sa: N4842, 32.8.1 Latches [thread.latch].
//
// TODO(luobogao): We do not perfectly match `std::latch` yet.
class Latch {
 public:
  explicit Latch(std::ptrdiff_t count);

  // Decrement internal counter. If it reaches zero, wake up all waiters.
  void count_down(std::ptrdiff_t update = 1);

  // Test if the latch's internal counter has become zero.
  bool try_wait() const noexcept;

  // Wait until `Latch`'s internal counter reached zero.
  void wait() const;

  // Extension to `std::latch`.
  template <class Rep, class Period>
  bool wait_for(const std::chrono::duration<Rep, Period>& timeout) {
    std::unique_lock lk(m_);
    FLARE_CHECK_GE(count_, 0);
    return cv_.wait_for(lk, timeout, [this] { return count_ == 0; });
  }

  // Extension to `std::latch`.
  template <class Clock, class Duration>
  bool wait_until(const std::chrono::time_point<Clock, Duration>& timeout) {
    std::unique_lock lk(m_);
    FLARE_CHECK_GE(count_, 0);
    return cv_.wait_until(lk, timeout, [this] { return count_ == 0; });
  }

  // Shorthand for `count_down(); wait();`
  void arrive_and_wait(std::ptrdiff_t update = 1);

 private:
  mutable std::mutex m_;
  mutable std::condition_variable cv_;
  std::ptrdiff_t count_;
};

}  // namespace flare

#endif  // FLARE_BASE_THREAD_LATCH_H_
