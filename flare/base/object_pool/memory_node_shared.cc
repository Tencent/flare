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

#include "flare/base/object_pool/memory_node_shared.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <vector>

#include "flare/base/align.h"
#include "flare/base/deferred.h"
#include "flare/base/demangle.h"
#include "flare/base/erased_ptr.h"
#include "flare/base/internal/annotation.h"
#include "flare/base/internal/background_task_host.h"
#include "flare/base/internal/cpu.h"
#include "flare/base/internal/doubly_linked_list.h"
#include "flare/base/internal/time_keeper.h"
#include "flare/base/likely.h"
#include "flare/base/logging.h"
#include "flare/base/never_destroyed.h"
#include "flare/base/thread/spinlock.h"

using namespace std::literals;

namespace flare::object_pool::detail::memory_node_shared {

struct Block {
  flare::internal::DoublyLinkedListEntry chain;
  std::chrono::steady_clock::time_point transferred{ReadCoarseSteadyClock()};
  std::vector<ErasedPtr> objects;
};

struct alignas(hardware_destructive_interference_size) Bucket {
  // NOT protected by `lock`.
  std::atomic<std::chrono::steady_clock::duration> last_wash{};

  // NOT protected by `lock`.
  //
  // This flag prevents multiple thread from flushing the bucket concurrently,
  // which introduces contention.
  std::atomic<bool> flushing{};

  // Protects both primary & secondary cache.
  Spinlock lock;

  // Object are normally cached here, except for the last `kLowWaterMark`
  // objects.
  //
  // For those "backup" objects, see below.
  flare::internal::DoublyLinkedList<Block, &Block::chain> primary_cache;

  // Life saver.
  //
  // We always keep at most `kLowWaterMark` objects here. Objects kept here are
  // not subject to washout.
  //
  // The reason why we can't keep them in `primary_cache` (see above) as well it
  // that objects in the "secondary" cache are likely to be idle for long. Were
  // they placed in `primary_cache`, they'll likely to be victims of our idle
  // object elimination algorithm.
  flare::internal::DoublyLinkedList<Block, &Block::chain> secondary_cache;

  // Read only.
  std::size_t secondary_cache_size;

  // Grab up an object.
  std::unique_ptr<Block> Pop() noexcept {
    std::scoped_lock _(lock);
    if (auto rc = primary_cache.pop_back()) {
      return std::unique_ptr<Block>(rc);
    }
    if (auto rc = secondary_cache.pop_back()) {
      return std::unique_ptr<Block>(rc);
    }
    return nullptr;
  }

  // Return an object.
  void Push(std::unique_ptr<Block> block) noexcept {
    std::scoped_lock _(lock);
    // It's always returned to the primary cache. Moving it to secondary cache
    // when necessary is done when washing out the primary cache.
    primary_cache.push_back(block.release());
  }

  ~Bucket() {
    while (Pop()) {
      // NOTHING.
    }
    FLARE_CHECK(primary_cache.empty());
    FLARE_CHECK(secondary_cache.empty());
  }
};

// Free objects can be costly (especially if the batch size is large or the
// object is costly to free, so cap it.).
constexpr auto kMinimumWashInterval = 50ms;
constexpr auto kMaximumFreePerRound = 4;  // In terms of `Block`s.

// We prefer to free objects in asynchronous fashion, for synchronous run of
// `UnsafeWashOutBucket`, we allow up to so many seconds delay (high water mark
// is still respected.).
constexpr auto kSynchronousFreeDelay = 2s;

ExposedMetrics<std::uint64_t, flare::detail::TscToDuration<std::uint64_t>>
    sync_washout_delay("flare/object_pool/node_shared/sync_washout_delay");

namespace {

std::size_t GetCurrentNodeIndexApprox() {
  FLARE_INTERNAL_TLS_MODEL thread_local std::chrono::steady_clock::time_point
      next_update;
  FLARE_INTERNAL_TLS_MODEL thread_local std::size_t node{};
  if (FLARE_UNLIKELY(next_update < ReadCoarseSteadyClock())) {
    next_update = ReadCoarseSteadyClock() + 1s;
    node = internal::numa::GetCurrentNodeIndex();
  }
  return node;
}

std::unique_ptr<Bucket[]> CreateBuckets(std::size_t count,
                                        std::size_t secondary_cache_size) {
  auto rc = std::make_unique<Bucket[]>(count);
  for (std::size_t index = 0; index != count; ++index) {
    rc[index].secondary_cache_size = secondary_cache_size;
  }
  return rc;
}

void UnsafeWashOutBucket(const TypeDescriptor& type, GlobalPoolDescriptor* pool,
                         Bucket* bucket,
                         std::chrono::nanoseconds extra_idle_tolerance) {
  flare::internal::DoublyLinkedList<Block, &Block::chain> destroying;
  auto now = ReadCoarseSteadyClock();
  bool piling_up = false;

  // Objects that cannot be moved to secondary cache are freed here.
  ScopedDeferred _([&] {
    while (!destroying.empty()) {
      std::unique_ptr<Block> freeing{destroying.pop_back()};
      (*pool->alive_objects)->Subtract(freeing->objects.size());
    }
  });

  {
    auto expires_at = now - (pool->max_idle + extra_idle_tolerance);
    std::scoped_lock _(bucket->lock);

    // Here we only free objects in `primary_cache`, and all of them are subject
    // to elimination. (for `kLowWaterMark` options, it's taken cared of by
    // `secondary_cache`).
    while (!bucket->primary_cache.empty() &&
           // High water-mark is a hard limit and may not be exceeded.
           (bucket->primary_cache.size() > pool->max_blocks_per_node ||
            (bucket->primary_cache.front().transferred <= expires_at &&
             destroying.size() < kMaximumFreePerRound))) {
      // Try move it to secondary cache first. This saves us a (presumably)
      // costly object destruction.
      if (bucket->secondary_cache.size() < bucket->secondary_cache_size) {
        bucket->secondary_cache.push_back(bucket->primary_cache.pop_front());
      } else {
        // The secondary cache is full as well, we're out of luck.
        destroying.push_back(bucket->primary_cache.pop_front());
      }
    }

    // How can we try to free something without filling up secondary cache?
    FLARE_CHECK(!(!destroying.empty() && bucket->secondary_cache.size() !=
                                             bucket->secondary_cache_size));

    // This flag is set if the primary cache will cost more than 30s to be
    // complete freed and there are still something left for us to free.
    //
    // Something must have went wrong then.
    piling_up = (bucket->primary_cache.size() >
                 1s / kMinimumWashInterval * kMaximumFreePerRound * 30) &&
                destroying.size() >= kMaximumFreePerRound;
  }

  // The cache is piling up.
  if (piling_up) {
    // Trigger an immediately washout (the next time `Put()` is called).
    bucket->last_wash.store(std::chrono::steady_clock::duration{},
                            std::memory_order_relaxed);
    FLARE_LOG_WARNING_EVERY_SECOND(
        "The primary cache for object type [{}] is piling up, you really have "
        "something to deal with. I'll free the cache excessively. Performance "
        "will degrade.",
        Demangle(type.type.GetRuntimeTypeIndex().name()));
  }

  // We've reached high water mark?
  if (destroying.size() > kMaximumFreePerRound) {
    FLARE_LOG_WARNING_EVERY_SECOND(
        "Object cache for object type [{}] overflowed. Freeing the cache "
        "excessively. Performance will suffer.",
        Demangle(type.type.GetRuntimeTypeIndex().name()));
  }
}

// In case the thread-local cache is always hit, we won't be able to free the
// cache periodically (which is only done in slow path). In this case we set a
// timer to trigger washout periodically.
class PeriodicalCacheWasher {
 public:
  static PeriodicalCacheWasher* Instance() {
    static NeverDestroyed<PeriodicalCacheWasher> washer;
    return washer.Get();
  }

  void Start() {
    timer_id_ = flare::internal::TimeKeeper::Instance()->AddTimer(
        {}, kMinimumWashInterval, [this](auto) { TriggerWashOut(); }, false);
  }
  void Stop() { flare::internal::TimeKeeper::Instance()->KillTimer(timer_id_); }

  void RegisterPool(GlobalPoolDescriptor* pool) {
    std::scoped_lock _(lock_);
    pools_.emplace_back(pool);
  }

 private:
  void TriggerWashOut() {
    auto pools_cp = [&] {
      std::scoped_lock _(lock_);
      return pools_;
    }();

    for (auto&& pool : pools_cp) {
      for (std::size_t i = 0;
           i != flare::internal::numa::GetNumberOfNodesAvailable(); ++i) {
        auto wash_cb = [pool, bucket = &pool->per_node_cache[i]] {
          if (!bucket->flushing.exchange(true, std::memory_order_relaxed)) {
            auto now = ReadCoarseSteadyClock().time_since_epoch();
            auto prev_wash = now - kMinimumWashInterval;
            while (bucket->last_wash.load(std::memory_order_relaxed) <
                   prev_wash) {
              bucket->last_wash.store(now, std::memory_order_relaxed);
              UnsafeWashOutBucket(*pool->type, pool, bucket,
                                  0s /* No extra idle timeout tolerance. */);
            }
            bucket->flushing.store(false, std::memory_order_relaxed);
          }
        };
        flare::internal::BackgroundTaskHost::Instance()->Queue(
            flare::internal::numa::GetNodeId(i), wash_cb);
      }
    }
  }

 private:
  std::uint64_t timer_id_{};
  std::mutex lock_;
  std::vector<GlobalPoolDescriptor*> pools_;
};

}  // namespace

GlobalPoolDescriptor::~GlobalPoolDescriptor() = default;

LocalPoolDescriptor::~LocalPoolDescriptor() { tls_destroyed = true; }

std::unique_ptr<GlobalPoolDescriptor> CreateGlobalPoolDescriptor(
    const TypeDescriptor& desc, std::size_t min_blocks_per_node,
    std::size_t max_blocks_per_node, std::chrono::nanoseconds max_idle,
    std::size_t transfer_threshold, std::size_t transfer_batch_size) {
  auto type_name = Demangle(desc.type.GetRuntimeTypeIndex().name());
  auto metrics_prefix = "flare/object_pool/node_shared/" + type_name + "/";
  return std::unique_ptr<GlobalPoolDescriptor>(new GlobalPoolDescriptor{
      .type = &desc,
      .min_blocks_per_node = min_blocks_per_node,
      .max_blocks_per_node = max_blocks_per_node,
      .max_idle = max_idle,
      .transfer_threshold = transfer_threshold,
      .transfer_batch_size = transfer_batch_size,
      .per_node_cache =
          CreateBuckets(flare::internal::numa::GetNumberOfNodesAvailable(),
                        min_blocks_per_node),
      .tls_cache_miss = std::make_unique<ExposedGauge<std::uint64_t>>(
          metrics_prefix + "tls_cache_miss"),
      .hard_cache_miss = std::make_unique<ExposedGauge<std::uint64_t>>(
          metrics_prefix + "hard_cache_miss"),
      .alive_objects = std::make_unique<ExposedGauge<std::int64_t>>(
          metrics_prefix + "alive_objects"),
      .slow_get_latency =
          std::make_unique<flare::internal::ExposedMetricsInTsc>(
              metrics_prefix + "slow_get_latency"),
      .slow_put_latency =
          std::make_unique<flare::internal::ExposedMetricsInTsc>(
              metrics_prefix + "slow_put_latency"),
  });
}

void RegisterGlobalPoolDescriptor(GlobalPoolDescriptor* desc) {
  PeriodicalCacheWasher::Instance()->RegisterPool(desc);
}

LocalPoolDescriptor CreateLocalPoolDescriptor(GlobalPoolDescriptor* gp_desc) {
  return LocalPoolDescriptor{.objects =
                                 FixedVector(gp_desc->transfer_threshold)};
}

void StartPeriodicalCacheWasher() {
  PeriodicalCacheWasher::Instance()->Start();
}

void StopPeriodicalCacheWasher() { PeriodicalCacheWasher::Instance()->Stop(); }

std::vector<Function<void()>>* GetEarlyInitializationRegistry() {
  static NeverDestroyed<std::vector<Function<void()>>> registry;
  return registry.Get();
}

void RegisterEarlyInitializationCallback(Function<void()> cb) {
  GetEarlyInitializationRegistry()->push_back(std::move(cb));
}

void EarlyInitializeForCurrentThread() {
  for (auto&& e : *GetEarlyInitializationRegistry()) {
    e();
  }
}

void* GetSlow(const TypeDescriptor& type, GlobalPoolDescriptor* global,
              LocalPoolDescriptor* local) {
  ScopedDeferred _([&, tsc = ReadTsc()] {
    (*global->slow_get_latency)->Report(TscElapsed(tsc, ReadTsc()));
  });

  (*global->tls_cache_miss)->Add(1);
  // Let's see if we can transfer something from shared cache.
  auto&& bucket = global->per_node_cache[GetCurrentNodeIndexApprox()];
  auto transferred = bucket.Pop();
  if (!transferred) {
    (*global->hard_cache_miss)->Add(1);
    (*global->alive_objects)->Add(1);
    return type.create();  // Bad luck.
  }
  auto rc = std::move(transferred->objects.back());
  transferred->objects.pop_back();
  for (auto&& e : transferred->objects) {
    local->objects.emplace_back(e.Get(), e.GetDeleter());
    (void)e.Leak();
  }
  return rc.Leak();
}

void PutSlow(const TypeDescriptor& type, GlobalPoolDescriptor* global,
             LocalPoolDescriptor* local, void* ptr) {
  // We don't want to bother touching anything else if the thread (and likely,
  // the whole program) is leaving.
  if (FLARE_UNLIKELY(LocalPoolDescriptor::tls_destroyed)) {
    type.destroy(ptr);
    return;
  }

  ScopedDeferred _([&, tsc = ReadTsc()] {
    (*global->slow_put_latency)->Report(TscElapsed(tsc, ReadTsc()));
  });

  if (local->objects.size() >= global->transfer_threshold) {
    auto&& bucket = global->per_node_cache[GetCurrentNodeIndexApprox()];

    // We'll check if the shared bucket needs washing when leaving.
    ScopedDeferred _([&] {
      auto now = ReadCoarseSteadyClock().time_since_epoch();
      auto prev_wash = now - kMinimumWashInterval;
      if (!bucket.flushing.exchange(true, std::memory_order_relaxed)) {
        ScopedDeferred __([start_tsc = ReadTsc()] {
          sync_washout_delay->Report(TscElapsed(start_tsc, ReadTsc()));
        });

        std::unique_lock lk(bucket.lock);

        while (bucket.last_wash.load(std::memory_order_relaxed) < prev_wash ||
               bucket.primary_cache.size() > global->max_blocks_per_node) {
          if (bucket.primary_cache.size() > global->max_blocks_per_node) {
            // Triggered by high water-mark then.
            FLARE_LOG_WARNING_EVERY_SECOND(
                "High-water mark of object type [{}] reached. This can be "
                "caused if you're experiencing a peak in load, which is "
                "expected. However, if you're seeing this frequently, either "
                "the object pool water-mark is set too low, or something is "
                "going wrong.",
                Demangle(type.type.GetRuntimeTypeIndex().name()));
          }
          bucket.last_wash.store(now, std::memory_order_relaxed);
          lk.unlock();
          UnsafeWashOutBucket(type, global, &bucket, kSynchronousFreeDelay);
          lk.lock();
          if (bucket.primary_cache.size() > global->max_blocks_per_node) {
            FLARE_LOG_WARNING_EVERY_SECOND(
                "The object of type [{}] are piling up quickly. Freeing the "
                "cache again. Performance will suffer.",
                Demangle(type.type.GetRuntimeTypeIndex().name()));
          }  // The only reason the `while` will loop.
        }
        bucket.flushing.store(false, std::memory_order_relaxed);
      }
    });

    // TODO(luobogao): Does it make sense to pool `Block` (using thread-local
    // cache.)?
    auto transferring = std::make_unique<Block>();

    transferring->objects.reserve(global->transfer_batch_size);
    transferring->objects.emplace_back(ptr, type.destroy);
    FLARE_CHECK_GE(local->objects.size(), global->transfer_batch_size - 1);
    for (std::size_t index = 0; index != global->transfer_batch_size - 1;
         ++index) {
      auto&& current = local->objects.pop_back();
      transferring->objects.emplace_back(current->first, current->second);
    }

    bucket.Push(std::move(transferring));
  } else {
    FLARE_CHECK(!local->objects.full());
    local->objects.emplace_back(ptr, type.destroy);
  }
  FLARE_CHECK_LE(local->objects.size(), global->transfer_threshold);
}

}  // namespace flare::object_pool::detail::memory_node_shared
