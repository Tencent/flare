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

#ifndef FLARE_BASE_DOWN_CAST_H_
#define FLARE_BASE_DOWN_CAST_H_

#include <type_traits>

#include "flare/base/internal/logging.h"

// Usage:
//
// T* casted = down_cast<T>(some_ptr);
// const T* casted = down_cast<T>(some_constant_ptr);
// T* casted = down_cast<T>(some_ref);
// const T* casted = down_cast<T>(some_constant_ref);
//
// This method deviates from std's convention, the reasons are explained in
// `casting.h`

namespace flare {

// `ptr` may not be `nullptr`.
template <class T, class U,
          class R = std::conditional_t<std::is_const_v<U>, const T*, T*>>
inline R down_cast(U* ptr) {
  // Well, were here a `CHECK`, there's no point in using `down_cast` at all,
  // since it can't outperform `dynamic_cast` anyway.
  FLARE_DCHECK(dynamic_cast<R>(ptr));
  return static_cast<R>(ptr);
}

template <class T, class U>
inline auto down_cast(U& ref) {
  return flare::down_cast<T>(&ref);
}

}  // namespace flare

#endif  // FLARE_BASE_DOWN_CAST_H_
