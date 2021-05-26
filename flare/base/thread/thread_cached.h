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

#ifndef FLARE_BASE_THREAD_THREAD_CACHED_H_
#define FLARE_BASE_THREAD_THREAD_CACHED_H_

#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <utility>

#include "flare/base/thread/thread_local.h"

namespace flare {

// This class helps you optimizing read-mostly shared data access by caching
// data locally in TLS.
//
// Note that this class can cause excessive memory usage (as it caches the data
// one per thread). If you need to optimize large object access (for read-mostly
// scenario), consider using `Hazptr` instead (albeit with a slightly higher
// perf. overhead.). (Space/time tradeoff.)
template <class T>
class ThreadCached {
 public:
  template <class... Us>
  explicit ThreadCached(Us&&... args) : value_(std::forward<Us>(args)...) {}

  // `Get` tests if thread-local cached object is up-to-date, and uses
  // thread-local only if it is, avoiding touching internally shared mutex or
  // global data.
  //
  // If the cached object is out-of-date, slow path (acquiring global mutex and
  // update the cache) is taken instead.
  //
  // CAUTION: TWO CONSECUTIVE CALLS TO `Get()` CAN RETURN REF. TO DIFFERENT
  // OBJECTS. BESIDES, IF THIS IS THE CASE, THE FIRST REF. IS INVALIDATED BEFORE
  // THE SECOND CALL RETURNS.
  const T& NonIdempotentGet() const {
    auto p = tls_cache_.Get();
    if (FLARE_UNLIKELY(p->version !=
                       version_.load(std::memory_order_relaxed))) {
      return GetSlow();
    }
    return *p->object;
  }

  // Use `args...` to reinitialize value stored.
  //
  // Note that each call to `Emplace` will cause subsequent calls to `Get()` to
  // acquire internal mutex (once per thread). So don't call `Emplace` unless
  // the value is indeed changed.
  //
  // Calls to `Emplace` acquire internal mutex, it's slow.
  template <class... Us>
  void Emplace(Us&&... args) {
    std::scoped_lock _(lock_);
    value_ = T(std::forward<Us>(args)...);
    // `value_` is always accessed with `lock_` held, so we don't need extra
    // fence when accessing `version_`.
    version_.fetch_add(1, std::memory_order_relaxed);
  }

  // TODO(luobogao): Support for replacing value stored by functor's return
  // value, with internally lock held.

 private:
  const T& GetSlow() const;

 private:
  struct Cache {
    std::uint64_t version = 0;
    std::unique_ptr<T> object;
  };

  std::atomic<std::uint64_t> version_{1};  // Incremented each time `value_`
                                           // is changed.
  internal::ThreadLocalAlwaysInitialized<Cache> tls_cache_;

  // I do think it's possible to optimize the lock away with `Hazptr` and
  // seqlocks.
  mutable std::shared_mutex lock_;
  T value_;
};

// NOT inlined (to keep fast-path `Get()` small.).
template <class T>
const T& ThreadCached<T>::GetSlow() const {
  std::shared_lock _(lock_);
  auto p = tls_cache_.Get();
  p->version = version_.load(std::memory_order_relaxed);
  p->object = std::make_unique<T>(value_);
  return *p->object;
}

}  // namespace flare

#endif  // FLARE_BASE_THREAD_THREAD_CACHED_H_
