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

#ifndef FLARE_BASE_INTERNAL_COPYABLE_ATOMIC_H_
#define FLARE_BASE_INTERNAL_COPYABLE_ATOMIC_H_

#include <atomic>

namespace flare::internal {

// Make `std::atomic<T>` copyable.
template <class T>
class CopyableAtomic : public std::atomic<T> {
 public:
  CopyableAtomic() = default;
  /* implicit */ CopyableAtomic(T value) : std::atomic<T>(std::move(value)) {}

  constexpr CopyableAtomic(const CopyableAtomic& from)
      : std::atomic<T>(from.load()) {}

  constexpr CopyableAtomic& operator=(const CopyableAtomic& from) {
    store(from.load());
    return *this;
  }
};

}  // namespace flare::internal

#endif  // FLARE_BASE_INTERNAL_COPYABLE_ATOMIC_H_
