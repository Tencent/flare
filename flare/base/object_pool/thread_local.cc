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

#include "flare/base/object_pool/thread_local.h"

#include <algorithm>
#include <utility>

#include "flare/base/chrono.h"
#include "flare/base/deferred.h"

using namespace std::literals;

namespace flare::object_pool::detail::tls {

constexpr std::size_t kMinimumFreePerWash = 32;
constexpr auto kMinimumWashInterval = 5ms;

namespace {

std::size_t GetFreeCount(std::size_t upto) {
  return std::min(upto, std::max(upto / 2, kMinimumFreePerWash));
}

void WashOutCache(PoolDescriptor* pool) {
  auto now = ReadCoarseSteadyClock();
  if (now < pool->last_wash + kMinimumWashInterval) {
    return;  // We're called too frequently.
  } else {
    pool->last_wash = now;
  }

  auto&& primary = pool->primary_cache;
  auto&& secondary = pool->secondary_cache;
  auto move_to_secondary_or_free = [&](std::size_t count) {
    while (count--) {
      if (secondary.size() < pool->low_water_mark) {
        secondary.push_back(std::move(primary.front()));
      }
      primary.pop_front();
    }
  };

  // We've reached the high-water mark, free some objects.
  if (pool->primary_cache.size() > pool->high_water_mark) {
    auto upto =
        GetFreeCount(pool->primary_cache.size() - pool->high_water_mark);
    move_to_secondary_or_free(upto);
    if (upto == kMinimumFreePerWash) {
      return;  // We've freed enough objects then.
    }
  }

#ifndef NDEBUG
  std::size_t objects_had =
      pool->primary_cache.size() + pool->secondary_cache.size();
#endif

  // Let's see how many objects have been idle for too long.
  auto idle_objects = std::find_if(primary.begin(), primary.end(),
                                   [&](auto&& e) {
                                     return now - e.last_used < pool->max_idle;
                                   }) -
                      primary.begin();
  move_to_secondary_or_free(GetFreeCount(idle_objects));

#ifndef NDEBUG
  if (objects_had >= pool->low_water_mark) {
    FLARE_CHECK_GE(pool->primary_cache.size() + pool->secondary_cache.size(),
                   pool->low_water_mark);
  }
#endif
}

}  // namespace

void* Get(const TypeDescriptor& desc, PoolDescriptor* pool) {
  if (pool->primary_cache.empty()) {
    if (!pool->secondary_cache.empty()) {
      pool->primary_cache = std::move(pool->secondary_cache);
      // Reset the timestamp, otherwise they'll likely be moved to secondary
      // cache immediately.
      for (auto&& e : pool->primary_cache) {
        e.last_used = ReadCoarseSteadyClock();
      }
    } else {
      // We could just return the object just created instead of temporarily
      // push it into `primary_cache`. However, since we expect the pool should
      // satisfy most needs (i.e., this path should be seldom taken), this won't
      // hurt much.
      pool->primary_cache.push_back(
          TimestampedObject{.ptr = {desc.create(), desc.destroy},
                            .last_used = ReadCoarseSteadyClock()});
    }
  }
  auto rc = std::move(pool->primary_cache.back());
  pool->primary_cache.pop_back();
  return rc.ptr.Leak();
}

void Put(const TypeDescriptor& desc, PoolDescriptor* pool, void* ptr) {
  ScopedDeferred _([&] { WashOutCache(pool); });
  pool->primary_cache.push_back(TimestampedObject{
      .ptr = {ptr, desc.destroy}, .last_used = ReadCoarseSteadyClock()});
}

}  // namespace flare::object_pool::detail::tls
