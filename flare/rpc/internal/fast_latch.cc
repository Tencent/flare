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

#include "flare/rpc/internal/fast_latch.h"

namespace flare::rpc::detail {

void FastLatch::NotifySlow() noexcept {
  std::scoped_lock _(lock_);
  wake_up_ = true;
  cv_.notify_one();
}

void FastLatch::WaitSlow() noexcept {
  std::unique_lock lk(lock_);
  cv_.wait(lk, [&] { return wake_up_; });
}

}  // namespace flare::rpc::detail
