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

#ifndef FLARE_FIBER_CONDITION_VARIABLE_H_
#define FLARE_FIBER_CONDITION_VARIABLE_H_

#include <condition_variable>

#include "flare/fiber/detail/waitable.h"
#include "flare/fiber/mutex.h"

namespace flare::fiber {

// Analogous to `std::condition_variable`, but it's for fiber.
class ConditionVariable {
 public:
  // Wake up one waiter.
  void notify_one() noexcept;

  // Wake up all waiters.
  void notify_all() noexcept;

  // Wait until someone called `notify_xxx()`.
  void wait(std::unique_lock<Mutex>& lock);

  // Wait until `pred` is satisfied.
  template <class Predicate>
  void wait(std::unique_lock<Mutex>& lock, Predicate pred);

  // Wait until either someone notified us or `expires_in` has expired.
  //
  // I totally have no idea why Standard Committee didn't simply specify this
  // method to return `bool` instead.
  template <class Rep, class Period>
  std::cv_status wait_for(std::unique_lock<Mutex>& lock,
                          std::chrono::duration<Rep, Period> expires_in);

  // Wait until either `pred` is satisfied or `expires_in` has expired.
  template <class Rep, class Period, class Predicate>
  bool wait_for(std::unique_lock<Mutex>& lock,
                std::chrono::duration<Rep, Period> expires_in, Predicate pred);

  // Wait until either someone notified us or `expires_at` is reached.
  template <class Clock, class Duration>
  std::cv_status wait_until(
      std::unique_lock<Mutex>& lock,
      std::chrono::time_point<Clock, Duration> expires_at);

  // Wait until either `pred` is satisfied or `expires_at` is reached.
  template <class Clock, class Duration, class Pred>
  bool wait_until(std::unique_lock<Mutex>& lock,
                  std::chrono::time_point<Clock, Duration> expires_at,
                  Pred pred);

 private:
  detail::ConditionVariable impl_;
};

template <class Predicate>
void ConditionVariable::wait(std::unique_lock<Mutex>& lock, Predicate pred) {
  impl_.wait(lock, pred);
}

template <class Rep, class Period>
std::cv_status ConditionVariable::wait_for(
    std::unique_lock<Mutex>& lock,
    std::chrono::duration<Rep, Period> expires_in) {
  auto steady_timeout = ReadSteadyClock() + expires_in;
  return impl_.wait_until(lock, steady_timeout) ? std::cv_status::no_timeout
                                                : std::cv_status::timeout;
}

template <class Rep, class Period, class Predicate>
bool ConditionVariable::wait_for(std::unique_lock<Mutex>& lock,
                                 std::chrono::duration<Rep, Period> expires_in,
                                 Predicate pred) {
  auto steady_timeout = ReadSteadyClock() + expires_in;
  return impl_.wait_until(lock, steady_timeout, pred);
}

template <class Clock, class Duration>
std::cv_status ConditionVariable::wait_until(
    std::unique_lock<Mutex>& lock,
    std::chrono::time_point<Clock, Duration> expires_at) {
  auto steady_timeout = ReadSteadyClock() + (expires_at - Clock::now());
  return impl_.wait_until(lock, steady_timeout) ? std::cv_status::no_timeout
                                                : std::cv_status::timeout;
}

template <class Clock, class Duration, class Pred>
bool ConditionVariable::wait_until(
    std::unique_lock<Mutex>& lock,
    std::chrono::time_point<Clock, Duration> expires_at, Pred pred) {
  auto steady_timeout = ReadSteadyClock() + (expires_at - Clock::now());
  return impl_.wait_until(lock, steady_timeout, pred);
}

}  // namespace flare::fiber

#endif  // FLARE_FIBER_CONDITION_VARIABLE_H_
