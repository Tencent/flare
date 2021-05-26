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

#include "flare/base/internal/index_alloc.h"

namespace flare::internal {

std::size_t IndexAlloc::Next() {
  std::scoped_lock _(lock_);
  if (!recycled_.empty()) {
    auto rc = recycled_.back();
    recycled_.pop_back();
    return rc;
  }
  return current_++;
}

void IndexAlloc::Free(std::size_t index) {
  std::scoped_lock _(lock_);
  recycled_.push_back(index);
}

}  // namespace flare::internal
