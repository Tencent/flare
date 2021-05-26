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

#ifndef FLARE_BASE_THREAD_SEMAPHORE_H_
#define FLARE_BASE_THREAD_SEMAPHORE_H_

#include <chrono>
#include <cinttypes>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <type_traits>

namespace flare {

// @sa: https://en.cppreference.com/w/cpp/thread/counting_semaphore

// BasicCountingSemaphore is mostly used internally, to avoid code duplication
// between pthread / fiber version semaphore.
template <class Mutex, class ConditionVariable, std::ptrdiff_t kLeastMaxValue>
class BasicCountingSemaphore {
 public:
  explicit BasicCountingSemaphore(std::ptrdiff_t desired) : current_(desired) {}

  // Acquire / release semaphore, blocking.
  void acquire();
  void release(std::ptrdiff_t count = 1);

  // Non-blocking counterpart of `acquire`. This one fails immediately if the
  // semaphore can't be acquired.
  bool try_acquire() noexcept;

  // `acquire` with timeout.
  template <class Rep, class Period>
  bool try_acquire_for(const std::chrono::duration<Rep, Period>& expires_in);
  template <class Clock, class Duration>
  bool try_acquire_until(
      const std::chrono::time_point<Clock, Duration>& expires_at);

 private:
  std::mutex lock_;
  std::condition_variable cv_;
  std::uint32_t current_;
};
// Mimic of C++20 `std::counting_semaphore`. It's unfortunate that for the
// moment our compiler (libstdc++, to be precise) doesn't implement these
// classes yet.
template <std::ptrdiff_t kLeastMaxValue =
              std::numeric_limits<std::uint32_t>::max()>
class CountingSemaphore
    : public BasicCountingSemaphore<std::mutex, std::condition_variable,
                                    kLeastMaxValue> {
 public:
  explicit CountingSemaphore(std::ptrdiff_t desired)
      : BasicCountingSemaphore<std::mutex, std::condition_variable,
                               kLeastMaxValue>(desired) {}
};

// `BinarySemaphore` permits more optimization. But for the moment we just make
// it an alias to `CountingSemaphore<1>`.
using BinarySemaphore = CountingSemaphore<1>;

/////////////////////////////////
// Implementation goes below.  //
/////////////////////////////////

template <class Mutex, class ConditionVariable, std::ptrdiff_t kLeastMaxValue>
void BasicCountingSemaphore<Mutex, ConditionVariable,
                            kLeastMaxValue>::acquire() {
  std::unique_lock lk(lock_);
  cv_.wait(lk, [&] { return current_ != 0; });
  --current_;
  return;
}

template <class Mutex, class ConditionVariable, std::ptrdiff_t kLeastMaxValue>
void BasicCountingSemaphore<Mutex, ConditionVariable, kLeastMaxValue>::release(
    std::ptrdiff_t count) {
  std::scoped_lock lk(lock_);
  current_ += count;
  if (count == 1) {
    cv_.notify_one();
  } else {
    cv_.notify_all();
  }
}

template <class Mutex, class ConditionVariable, std::ptrdiff_t kLeastMaxValue>
bool BasicCountingSemaphore<Mutex, ConditionVariable,
                            kLeastMaxValue>::try_acquire() noexcept {
  std::scoped_lock _(lock_);
  if (current_) {
    --current_;
    return true;
  }
  return false;
}

template <class Mutex, class ConditionVariable, std::ptrdiff_t kLeastMaxValue>
template <class Rep, class Period>
bool BasicCountingSemaphore<Mutex, ConditionVariable, kLeastMaxValue>::
    try_acquire_for(const std::chrono::duration<Rep, Period>& expires_in) {
  std::unique_lock lk(lock_);
  if (!cv_.wait_for(lk, expires_in, [&] { return current_ != 0; })) {
    return false;
  }
  --current_;
  return true;
}

template <class Mutex, class ConditionVariable, std::ptrdiff_t kLeastMaxValue>
template <class Clock, class Duration>
bool BasicCountingSemaphore<Mutex, ConditionVariable, kLeastMaxValue>::
    try_acquire_until(
        const std::chrono::time_point<Clock, Duration>& expires_at) {
  std::unique_lock lk(lock_);
  if (!cv_.wait_for(lk, expires_at, [&] { return current_ != 0; })) {
    return false;
  }
  --current_;
  return true;
}

}  // namespace flare

#endif  // FLARE_BASE_THREAD_SEMAPHORE_H_
