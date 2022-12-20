// Copyright (C) 2022 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef FLARE_FIBER_BARRIER_H_
#define FLARE_FIBER_BARRIER_H_

#include <cstddef>
#include <mutex>

#include "flare/fiber/condition_variable.h"
#include "flare/fiber/mutex.h"

namespace flare::fiber {

namespace detail {

struct empty_completion {
  void operator()() noexcept {}
};

}  // namespace detail

// Analogous to `std::barrier`, but it's for fiber.
//
template <typename CompletionFunction = detail::empty_completion>
class Barrier {
 public:
  // TODO: static constexpr std::ptrdiff_t max() noexcept;

  struct arrival_token final {
   public:
    arrival_token(arrival_token&&) = default;
    arrival_token& operator=(arrival_token&&) = default;
    ~arrival_token() = default;

   private:
    friend class Barrier;
    explicit arrival_token(std::ptrdiff_t token = 0) : phase_(token) {}
    std::ptrdiff_t phase_;
  };

  explicit Barrier(std::ptrdiff_t count,
                   CompletionFunction completion = CompletionFunction())
      : count_(count), expected_(count), completion_(std::move(completion)) {}

  Barrier(Barrier const&) = delete;
  Barrier& operator=(Barrier const&) = delete;
  // Arrives at barrier and decrements the expected count
  arrival_token arrive(std::ptrdiff_t update = 1);
  // Blocks at the phase synchronization point until its phase completion step
  // is run
  void wait(arrival_token&& phase) const;
  // Arrives at barrier and decrements the expected count by one, then blocks
  // until current phase completes
  void arrive_and_wait() { wait(arrive()); }
  // Decrements both the initial expected count for subsequent phases and the
  // expected count for current phase by one
  void arrive_and_drop();

 private:
  arrival_token arrive_without_lock(std::ptrdiff_t update = 1);

 private:
  mutable Mutex lock_;
  mutable ConditionVariable cv_;
  std::ptrdiff_t count_;
  std::ptrdiff_t expected_;
  arrival_token current_;
  CompletionFunction completion_;
};

template <typename CompletionFunction>
typename Barrier<CompletionFunction>::arrival_token
Barrier<CompletionFunction>::arrive(std::ptrdiff_t update) {
  std::scoped_lock _(lock_);
  return arrive_without_lock(update);
}

template <typename CompletionFunction>
void Barrier<CompletionFunction>::arrive_and_drop() {
  std::scoped_lock _(lock_);
  --expected_;
  (void)arrive_without_lock(1);
}

template <typename CompletionFunction>
typename Barrier<CompletionFunction>::arrival_token
Barrier<CompletionFunction>::arrive_without_lock(std::ptrdiff_t update) {
  FLARE_CHECK_GE(count_, update);
  count_ -= update;
  const auto old_phase = current_.phase_;
  if (!count_) {
    completion_();
    current_.phase_++;
    count_ = expected_;
    cv_.notify_all();
  }
  return arrival_token(old_phase);
}

template <typename CompletionFunction>
void Barrier<CompletionFunction>::wait(arrival_token&& phase) const {
  std::unique_lock lk(lock_);
  FLARE_CHECK_GE(count_, 0);
  cv_.wait(lk, [this, phase = std::move(phase)] {
    return current_.phase_ != phase.phase_;
  });
}

}  // namespace flare::fiber

#endif  // FLARE_FIBER_BARRIER_H_
