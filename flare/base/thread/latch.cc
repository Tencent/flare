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

#include "flare/base/thread/latch.h"

#include "flare/base/internal/logging.h"

namespace flare {

Latch::Latch(std::ptrdiff_t count) : count_(count) {}

void Latch::count_down(std::ptrdiff_t update) {
  std::unique_lock lk(m_);
  FLARE_CHECK_GE(count_, update);
  count_ -= update;
  if (!count_) {
    cv_.notify_all();
  }
}

bool Latch::try_wait() const noexcept {
  std::scoped_lock _(m_);
  FLARE_CHECK_GE(count_, 0);
  return !count_;
}

void Latch::wait() const {
  std::unique_lock lk(m_);
  FLARE_CHECK_GE(count_, 0);
  return cv_.wait(lk, [this] { return count_ == 0; });
}

void Latch::arrive_and_wait(std::ptrdiff_t update) {
  count_down(update);
  wait();
}

}  // namespace flare
