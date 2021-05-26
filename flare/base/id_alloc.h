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

#ifndef FLARE_BASE_ID_ALLOC_H_
#define FLARE_BASE_ID_ALLOC_H_

#include <atomic>

#include "flare/base/internal/annotation.h"
#include "flare/base/likely.h"

// To allocate (non-contiguous non-duplicate) IDs, the implementation here
// performs well.

namespace flare::id_alloc {

template <class Traits>
class AllocImpl;

// `Traits` is defined as:
//
// struct Traits {
//   // (Integral) type of ID.
//   using Type = ...;
//
//   // Minimum / maximum value of ID.
//   //
//   // FIXME: `kMax` is always wasted. This is not a big deal for now and
//   // simplifies our implementation.
//   static constexpr auto kMin = ...;
//   static constexpr auto kMax = ...;
//
//   // For better performance, the implementation grabs a batch of IDs from
//   // global counter to its thread local cache. This parameter controls batch
//   // size.
//   static constexpr auto kBatchSize = ...;
// };
//
// This method differs from `IndexAlloc` in that `IndexAlloc` tries its best to
// reuse indices, in trade of performance. In contrast, this method does not
// try to reuse ID, for better performance.
template <class Traits>
inline auto Next() {
  return AllocImpl<Traits>::Next();
}

template <class Traits>
class AllocImpl {
 public:
  using Type = typename Traits::Type;

  // This method will likely be inlined.
  static Type Next() {
    // Let's see if our thread local cache can serve us.
    if (FLARE_LIKELY(current < max)) {
      return current++;
    }

    return SlowNext();
  }

 private:
  static Type SlowNext();

 private:
  static inline std::atomic<Type> global{Traits::kMin};
  FLARE_INTERNAL_TLS_MODEL static inline thread_local Type current = 0;
  // `max` shouldn't be reached.
  FLARE_INTERNAL_TLS_MODEL static inline thread_local Type max = 0;
};

template <class Traits>
typename AllocImpl<Traits>::Type AllocImpl<Traits>::SlowNext() {
  static_assert(Traits::kMin < Traits::kMax);
  static_assert(Traits::kBatchSize > 0);
  static_assert(Traits::kMin + Traits::kBatchSize < Traits::kMax,
                "Not supported due to implementation limitations.");
  static_assert(Traits::kBatchSize > 1,
                "Not supported due to implementation limitations.");

  // Get more IDs from global counter...
  Type read, next;
  do {
    read = global.load(std::memory_order_relaxed);
    FLARE_CHECK_GE(read, Traits::kMin);
    FLARE_CHECK_LE(read, Traits::kMax);
    current = read;
    if (read > Traits::kMax - Traits::kBatchSize /* Overflowing */) {
      next = Traits::kMin;
      max = Traits::kMax;
    } else {
      next = read + Traits::kBatchSize;
      max = next;
    }
  } while (
      !global.compare_exchange_weak(read, next, std::memory_order_relaxed));
  FLARE_CHECK_GE(max, Traits::kMin);
  FLARE_CHECK_LE(max, Traits::kMax);
  FLARE_CHECK_LT(current, max);
  FLARE_CHECK_GE(current, Traits::kMin);
  FLARE_CHECK_LE(current, Traits::kMax);

  // ... and retry.
  return Next();
}

}  // namespace flare::id_alloc

#endif  // FLARE_BASE_ID_ALLOC_H_
