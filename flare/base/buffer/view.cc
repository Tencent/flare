// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "flare/base/buffer/view.h"

#include <algorithm>
#include <utility>

namespace flare {

NoncontiguousBufferRandomView::NoncontiguousBufferRandomView()
    : NoncontiguousBufferRandomView(
          internal::EarlyInitConstant<NoncontiguousBuffer>()) {}

NoncontiguousBufferRandomView::NoncontiguousBufferRandomView(
    const NoncontiguousBuffer& buffer) {
  byte_size_ = buffer.ByteSize();
  auto offset = 0;
  for (auto iter = buffer.begin(); iter != buffer.end(); ++iter) {
    offsets_.emplace_back(offset, iter);
    offset += iter->size();
  }
  offsets_.emplace_back(offset, buffer.end());
  FLARE_CHECK_EQ(offset, buffer.ByteSize());
}

std::pair<std::size_t, NoncontiguousBuffer::const_iterator>
NoncontiguousBufferRandomView::FindSegmentMustSucceed(
    std::size_t offset) const noexcept {
  FLARE_CHECK_LE(offset, size(),
                 "Invalid offset [{}]. The buffer is only {} bytes long.",
                 offset, size());
  auto iter = std::upper_bound(
      offsets_.begin(), offsets_.end(), offset,
      [](auto value, auto&& entry) { return value < entry.first; });
  FLARE_CHECK(iter != offsets_.begin());
  --iter;
  FLARE_CHECK_LE(iter->first, offset);
  if (iter != offsets_.end() - 1) {
    FLARE_CHECK_LT(offset, iter->first + iter->second->size());
  }
  return *iter;
}

}  // namespace flare
