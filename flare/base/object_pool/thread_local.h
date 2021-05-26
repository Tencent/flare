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

#ifndef FLARE_BASE_OBJECT_POOL_THREAD_LOCAL_H_
#define FLARE_BASE_OBJECT_POOL_THREAD_LOCAL_H_

#include <chrono>
#include <deque>

#include "flare/base/chrono.h"
#include "flare/base/demangle.h"
#include "flare/base/erased_ptr.h"
#include "flare/base/internal/annotation.h"
#include "flare/base/logging.h"
#include "flare/base/object_pool/types.h"

// Extra requirement on `PoolTraits<T>`:
//
// ```
// Minimum free objects cached per thread.
//
// Note that that parameter only affects idle objects (@sa `kMaxIdle`). For
// objects that are fresh enough, they're always kept regardless of this
// parameter. (This means there are always at least `kLowWaterMark + 1`
// objects alive though, as cache washing is triggered by `Put`, and the
// object just `Put`-ed is always fresh, and won't be freed.).
//
// static constexpr std::size_t kLowWaterMark = ...;
//
// Maximum free objects cached per thread. If you don't want to set a threshold,
// use `std::numeric_limits<std::size_t>::max()`.
//
// This parameter also affects fresh objects. Objects are freed if there are
// more than `kHighWaterMark` objects alive regardless of their freshness.
//
// static constexpr std::size_t kHighWaterMark = ...;
//
// Quiet period before an object is eligible for removal (when there are more
// than `kLowWaterMark` idle objects are cached). Note that the implementation
// may delay object deallocation longer than this option.
//
// static constexpr std::chrono::nanoseconds kMaxIdle = ...;
//
// How often should the pool be purged. (TODO)
// ...
// ```

// Backend implementation for `PoolType::ThreadLocal`.
namespace flare::object_pool::detail::tls {

struct TimestampedObject {
  ErasedPtr ptr;
  std::chrono::steady_clock::time_point last_used;
};

struct PoolDescriptor {
  const std::size_t low_water_mark;
  const std::size_t high_water_mark;
  const std::chrono::nanoseconds max_idle;
  std::chrono::steady_clock::time_point last_wash{ReadCoarseSteadyClock()};

  // Objects in primary cache is washed out to `secondary_cache` if there's
  // still room.
  std::deque<TimestampedObject> primary_cache;

  // Objects here are not subject to washing out.
  std::deque<TimestampedObject> secondary_cache;
};

// `template <class T> inline thread_local PoolDescriptor pool` does not work:
//
// > redefinition of 'bool __tls_guard'
//
// I think it's a bug in GCC.
template <class T>
PoolDescriptor* GetThreadLocalPool() {
  static_assert(PoolTraits<T>::kHighWaterMark > PoolTraits<T>::kLowWaterMark,
                "You should leave some room between the water marks.");

  // Internally we always keep `kLowWaterMark` objects in secondary cache,
  // so the "effective" high water mark should subtract `kLowWaterMark`.
  constexpr auto kEffectiveHighWaterMark =
      PoolTraits<T>::kHighWaterMark - PoolTraits<T>::kLowWaterMark;

  FLARE_INTERNAL_TLS_MODEL thread_local PoolDescriptor pool = {
      .low_water_mark = PoolTraits<T>::kLowWaterMark,
      .high_water_mark = kEffectiveHighWaterMark,
      .max_idle = PoolTraits<T>::kMaxIdle};
  FLARE_DCHECK((pool.low_water_mark == PoolTraits<T>::kLowWaterMark) &&
                   (pool.high_water_mark == kEffectiveHighWaterMark) &&
                   (pool.max_idle == PoolTraits<T>::kMaxIdle),
               "You likely had an ODR-violation when customizing type [{}].",
               GetTypeName<T>());
  return &pool;
}

void* Get(const TypeDescriptor& desc, PoolDescriptor* pool);
void Put(const TypeDescriptor& desc, PoolDescriptor* pool, void* ptr);

}  // namespace flare::object_pool::detail::tls

#endif  // FLARE_BASE_OBJECT_POOL_THREAD_LOCAL_H_
