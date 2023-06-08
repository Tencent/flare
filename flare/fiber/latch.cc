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

#include "flare/fiber/latch.h"

namespace flare::fiber {

Latch::Latch(std::ptrdiff_t count) : count_(count) {}

void Latch::count_down(std::ptrdiff_t update) {
  bool count_is_zero = false;
  {
    std::scoped_lock _(lock_);
    FLARE_CHECK_GE(count_, update);
    count_is_zero = !(count_ -= update);
  }
  if (count_is_zero) {
    cv_.notify_all();
  }
}

bool Latch::try_wait() const noexcept {
  std::scoped_lock _(lock_);
  FLARE_CHECK_GE(count_, 0);
  return !count_;
}

void Latch::wait() const {
  std::unique_lock lk(lock_);
  FLARE_CHECK_GE(count_, 0);
  cv_.wait(lk, [&] { return count_ == 0; });
}

void Latch::arrive_and_wait(std::ptrdiff_t update) {
  count_down(update);
  wait();
}

}  // namespace flare::fiber
