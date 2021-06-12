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

#ifndef FLARE_BASE_OBJECT_POOL_MEMORY_NODE_SHARED_H_
#define FLARE_BASE_OBJECT_POOL_MEMORY_NODE_SHARED_H_

#include <chrono>
#include <limits>
#include <memory>
#include <mutex>

#include "flare/base/demangle.h"
#include "flare/base/exposed_var.h"
#include "flare/base/internal/annotation.h"
#include "flare/base/likely.h"
#include "flare/base/never_destroyed.h"
#include "flare/base/object_pool/types.h"

// Extra requirement on `PoolTraits<T>`:
//
// ```
// Minimum number of objects in per node cache. Note that objects that have not
// been returned to the shared cache (e.g., in thread local cache) is not
// counted.
//
// Internally this number of rounded to a multiple of `kTransferBatchSize`.
//
// This parameter should be *significantly* greater than `kTransferBatchSize`,
// otherwise you risk allocating too many objects in each thread (as there were
// no object in the shared pool) and then bursting destroying then (as there
// would be too many objects in the shared pool). This can severely hurts
// performance. (Much worse than lock contention on the shared pool.)
//
// static constexpr std::size_t kLowWaterMark = ...;
//
// Maximum number of objects in per node cache. Objects in thread local cache is
// not counted.
//
// Rounded to a multiple of `kTransferBatchSize` internally.
//
// `kMaxIdle` (see below) is not considered if number of alive objects exceeds
// this limit.
//
// static constexpr std::size_t kHighWaterMark = ...;
//
// Minimum grace period that must have passed before an object is considered
// eligible for recycling (if the number of alive objects in shared cache does
// not exceed `kHighWaterMark`).
//
// static const std::chrono::nanoseconds kMaxIdle = ...;
//
// We also maintain a thread-local object cache for each thread. Before
// transferring objects from to shared cache (i.e., the buckets),
// `kMinimumThreadCacheSize` objects are kept locally.
//
// To disable thread-local cache, set it to 0. (The object pool will still cache
// up to `kTransferBatchSize - 1` objects before the transfer happens.).
//
// static constexpr std::size_t kMinimumThreadCacheSize = ...;
//
// For better performance, objects are transferred from thread-local cache to
// buckets in batches. This parameter specifies batch size. (If objects in
// thread local cache is not sufficient to form a batch, they're kept
// locally.).
//
// static constexpr std::size_t kTransferBatchSize = ...;
// ```

// `PoolType::MemoryNodeShared`.
namespace flare::object_pool::detail::memory_node_shared {

struct Bucket;

// Global (to all scheduling group) pool descriptor.
struct GlobalPoolDescriptor {
  const TypeDescriptor* type;
  const std::size_t min_blocks_per_node;
  const std::size_t max_blocks_per_node;  // After substracting low water-mark.
  const std::chrono::nanoseconds max_idle;
  const std::size_t transfer_threshold;
  const std::size_t transfer_batch_size;

  std::unique_ptr<Bucket[]> per_node_cache;

  // Below are exported metrics for perf. analysis.

  // Thread-local cache miss.
  std::unique_ptr<ExposedGauge<std::uint64_t>> tls_cache_miss;
  // Miss in all-level cache.
  std::unique_ptr<ExposedGauge<std::uint64_t>> hard_cache_miss;
  // Number of alive objects.
  std::unique_ptr<ExposedGauge<std::int64_t>> alive_objects;
  // Latency of slow path.
  std::unique_ptr<flare::internal::ExposedMetricsInTsc> slow_get_latency,
      slow_put_latency;

  // This is required so as not to define `Bucket` here.
  ~GlobalPoolDescriptor();
};

// Thread-local object cache.
struct LocalPoolDescriptor {
  // See comments on `FixedVector` for the reason why `std::vector<...>` is not
  // used here.
  FixedVector objects;

  // `tls_destroyed` is set once thread-local pool is destroyed. This is needed
  // for handling object recycling when current thread is leaving. If we're
  // called after the thread local pool has been destroyed, the object must be
  // freed immediately instead of being put into (already-destroyed) thread
  // local pool.
  ~LocalPoolDescriptor();
  FLARE_INTERNAL_TLS_MODEL inline static thread_local bool tls_destroyed =
      false;
};

std::unique_ptr<GlobalPoolDescriptor> CreateGlobalPoolDescriptor(
    const TypeDescriptor& desc, std::size_t min_blocks_per_node,
    std::size_t max_blocks_per_node, std::chrono::nanoseconds max_idle,
    std::size_t transfer_threshold, std::size_t transfer_batch_size);
void RegisterGlobalPoolDescriptor(GlobalPoolDescriptor* desc);

LocalPoolDescriptor CreateLocalPoolDescriptor(GlobalPoolDescriptor* gp_desc);

// Start / stop background task for periodically washing object cache.
void StartPeriodicalCacheWasher();
void StopPeriodicalCacheWasher();

// Register a callback that will be called if early initialization is performed.
//
// @sa: `EarlyInitializeForCurrentThread` for early initialization.
void RegisterEarlyInitializationCallback(Function<void()> cb);

// Initialize this object pool for the calling thread.
//
// Object pool initialization requires some amount of stack storage. If the user
// is using an extremely small stack (e.g., system fiber, @sa: `flare/fiber`),
// it may want to finish initialization at convenient time.
void EarlyInitializeForCurrentThread();

template <class T>
GlobalPoolDescriptor* GetGlobalPoolDescriptor() {
  static_assert(
      PoolTraits<T>::kLowWaterMark == std::numeric_limits<std::size_t>::max() ||
          PoolTraits<T>::kLowWaterMark % PoolTraits<T>::kTransferBatchSize == 0,
      "You should specify `kLowWaterMark` as a multiple of "
      "`kTransferBatchSize`.");
  static_assert(
      PoolTraits<T>::kHighWaterMark ==
              std::numeric_limits<std::size_t>::max() ||
          PoolTraits<T>::kHighWaterMark % PoolTraits<T>::kTransferBatchSize ==
              0,
      "You should specify `kHighWaterMark` as a multiple of "
      "`kTransferBatchSize`.");
  constexpr auto kMinBlocksPerMemoryNode =
      PoolTraits<T>::kLowWaterMark / PoolTraits<T>::kTransferBatchSize;
  constexpr auto kMaxBlocksPerMemoryNode =
      PoolTraits<T>::kHighWaterMark / PoolTraits<T>::kTransferBatchSize -
      kMinBlocksPerMemoryNode;
  constexpr auto kTransferThreshold = PoolTraits<T>::kTransferBatchSize +
                                      PoolTraits<T>::kMinimumThreadCacheSize -
                                      1;

  static NeverDestroyed<std::unique_ptr<GlobalPoolDescriptor>> desc{
      CreateGlobalPoolDescriptor(*GetTypeDesc<T>(), kMinBlocksPerMemoryNode,
                                 kMaxBlocksPerMemoryNode,
                                 PoolTraits<T>::kMaxIdle, kTransferThreshold,
                                 PoolTraits<T>::kTransferBatchSize)};

  static std::once_flag gp_reg;
  // Global pool descriptor is never unregistered as it's never destroyed.
  std::call_once(gp_reg, [&] { RegisterGlobalPoolDescriptor(&**desc); });

  FLARE_DCHECK(
      ((*desc)->min_blocks_per_node == kMinBlocksPerMemoryNode) &&
          ((*desc)->max_blocks_per_node == kMaxBlocksPerMemoryNode) &&
          ((*desc)->max_idle == PoolTraits<T>::kMaxIdle) &&
          ((*desc)->transfer_threshold == kTransferThreshold) &&
          ((*desc)->transfer_batch_size == PoolTraits<T>::kTransferBatchSize),
      "You likely had an ODR-violation when customizing type [{}].",
      GetTypeName<T>());
  return &**desc;
}

template <class T>
LocalPoolDescriptor* GetLocalPoolDescriptor() {
  thread_local LocalPoolDescriptor desc =
      CreateLocalPoolDescriptor(GetGlobalPoolDescriptor<T>());
  return &desc;
}

struct Descriptors {
  const TypeDescriptor* type;
  GlobalPoolDescriptor* global;
  LocalPoolDescriptor* local;
};

// Template thread-local variable does not work in GCC 8.2, unfortunately. IIRC
// there's a PR in their bugzilla tracking this issue but I can't find it for
// the moment.
//
// Were template thread-local variable usable, we can eliminate the branch for
// TLS initialization.
//
// template <class T>
// inline thread_local Descriptors descriptors = {GetTypeDesc<T>(),
//                                                GetGlobalPoolDescriptor<T>(),
//                                                GetLocalPoolDescriptor<T>()};

template <class T>
FLARE_INTERNAL_TLS_MODEL inline thread_local Descriptors descriptors_ptr{};

// To keep `GetDescriptors` small, we move initialization code out.
template <class T>
[[gnu::noinline]] void InitializeDescriptorsSlow() {
  descriptors_ptr<T> = {GetTypeDesc<T>(), GetGlobalPoolDescriptor<T>(),
                        GetLocalPoolDescriptor<T>()};
}

void* GetSlow(const TypeDescriptor& type, GlobalPoolDescriptor* global,
              LocalPoolDescriptor* local);
void PutSlow(const TypeDescriptor& type, GlobalPoolDescriptor* global,
             LocalPoolDescriptor* local, void* ptr);

template <class T>
void InitantiateEarlyInitializer();

template <class T>
[[gnu::noinline]] void* InitializeOptAndGetSlow() {
  if (FLARE_UNLIKELY(!descriptors_ptr<T>.type)) {
    InitantiateEarlyInitializer<T>();
    InitializeDescriptorsSlow<T>();
  }
  auto&& [desc, global, local] = descriptors_ptr<T>;
  return GetSlow(*desc, global, local);
}

template <class T>
[[gnu::noinline]] void InitializeOptAndPutSlow(void* ptr) {
  if (FLARE_UNLIKELY(!descriptors_ptr<T>.type)) {
    InitializeDescriptorsSlow<T>();
  }
  auto&& [desc, global, local] = descriptors_ptr<T>;
  return PutSlow(*desc, global, local, ptr);
}

template <class T>
inline void* Get() {
  auto&& local = descriptors_ptr<T>.local;
  if (FLARE_LIKELY(local /* Initialized */ && !local->objects.empty())) {
    // Thread local cache hit.
    return local->objects.pop_back();
  }
  return InitializeOptAndGetSlow<T>();
}

template <class T>
inline void Put(void* ptr) {
  auto&& [desc, global, local] = descriptors_ptr<T>;
  if (FLARE_LIKELY(local /* Initialized */ && !local->objects.full())) {
    local->objects.emplace_back(ptr);
    return;
  }
  return InitializeOptAndPutSlow<T>(ptr);
}

// Instantiation leads to registration of `T` to early initialization callback
// registry.
template <class T>
struct EarlyInitializationRegisterer {
  EarlyInitializationRegisterer() {
    RegisterEarlyInitializationCallback([] {
      InitantiateEarlyInitializer<T>();
      InitializeDescriptorsSlow<T>();
    });
  }
};

template <class T>
inline EarlyInitializationRegisterer<T> early_init_registerer;

template <class T>
[[gnu::noinline, gnu::cold]] void InitantiateEarlyInitializer() {
  // Instantiates `early_init_registerer`. This object will be initialized at
  // program startup, and register early initializer for us.
  [[maybe_unused]] static auto ptr = &early_init_registerer<T>;
}

}  // namespace flare::object_pool::detail::memory_node_shared

#endif  // FLARE_BASE_OBJECT_POOL_MEMORY_NODE_SHARED_H_
