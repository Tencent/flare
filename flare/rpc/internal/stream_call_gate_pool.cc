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

#include "flare/rpc/internal/stream_call_gate_pool.h"

#include <memory>
#include <mutex>
#include <utility>

#include "thirdparty/gflags/gflags.h"

#include "flare/base/exposed_var.h"
#include "flare/base/hazptr.h"
#include "flare/base/internal/hash_map.h"
#include "flare/base/random.h"
#include "flare/base/thread/latch.h"
#include "flare/fiber/runtime.h"
#include "flare/fiber/this_fiber.h"
#include "flare/io/event_loop.h"
#include "flare/rpc/internal/stream_call_gate.h"

DEFINE_int32(flare_rpc_client_max_connections_per_server, 8,
             "Maximum connections per server. This number is round down to "
             "number of worker groups internally. This option only affects "
             "connections to server whose protocol supports multiplexing. Note "
             "that if you're using two different protocols to call a server, "
             "the connections are counted separately (i.e., there will be at "
             "most as two times connections as the limit specified here.).");
DEFINE_int32(flare_rpc_client_remove_idle_connection_interval, 15,
             "Interval, in seconds, between to run of removing client-side "
             "idle connections.");
// The client must close the connection before the server, otherwise we risk
// using a connection that has been (or, is being) closed by the server.
DEFINE_int32(
    flare_rpc_client_connection_max_idle, 45,
    "Time period before recycling a client-side idle connection, in seconds.");

using namespace std::literals;

namespace flare {

bool operator<(const Endpoint& left, const Endpoint& right) {
  if (left.Length() != right.Length()) {
    return left.Length() < right.Length();
  }
  // Address family is compared below.
  return memcmp(left.Get(), right.Get(), left.Length()) < 0;
}

}  // namespace flare

namespace flare::rpc::internal {

namespace {

// To save us from declare copy & move ctor / assignment for
// `StreamCallGateEntry`.
template <class T>
struct CopyableAtomic : public std::atomic<T> {
  using std::atomic<T>::atomic;

  CopyableAtomic(const CopyableAtomic& other)
      : std::atomic<T>(other.load(std::memory_order_relaxed)) {}

  CopyableAtomic& operator=(const CopyableAtomic& other) {
    this->store(other.load(std::memory_order_relaxed),
                std::memory_order_relaxed);
    return *this;
  }
};

struct EndpointHash {
  std::size_t operator()(const Endpoint& ep) const {
    return flare::internal::Hash<std::string_view>{}(
        {reinterpret_cast<const char*>(ep.Get()), ep.Length()});
  }
};

struct StreamCallGateEntry {
  CopyableAtomic<std::chrono::nanoseconds> last_used_since_epoch;
  RefPtr<StreamCallGate> gate;
};

std::atomic<bool> stopped = false;
std::shared_mutex call_gate_pool_lock;
flare::internal::HashMap<std::string,
                         std::unique_ptr<StreamCallGatePool>>
    call_gate_pools;  // @sa `GetGlobalStreamCallGatePool` for map's key.

void PurgeGatesUnsafe(
    flare::internal::HashMap<Endpoint, std::vector<StreamCallGateEntry>,
                             EndpointHash>* pool,
    std::vector<StreamCallGateEntry>* destroying) {
  auto now = ReadCoarseSteadyClock().time_since_epoch();
  for (auto giter = pool->begin(); giter != pool->end();) {
    auto&& entries = giter->second;

    // Do NOT use `std::remove_if` to remove idle gates, otherwise the gates
    // to be removed is freed by `std::remove_if` (as a side effect of its
    // calls to `std::move`.).
    for (auto iter = entries.begin(); iter != entries.end(); ++iter) {
      FLARE_CHECK(iter->gate);
      if (iter->last_used_since_epoch.load(std::memory_order_relaxed) +
              FLAGS_flare_rpc_client_connection_max_idle * 1s <
          now) {
        destroying->push_back(std::move(*iter));
        FLARE_CHECK(!iter->gate);
      }
    }
    entries.erase(std::remove_if(entries.begin(), entries.end(),
                                 [&](auto&& e) { return !e.gate; }),
                  entries.end());

    // If there's no gate left, remove the key altogether.
    if (entries.empty()) {
      giter = pool->erase(giter);
    } else {
      ++giter;
    }
  }
}

ExposedCounter<std::uint64_t> new_conn_creation_in_shared_pool(
    "flare/rpc/client/new_conn_creation_in_shared_pool");

}  // namespace

// Pool for shared call gates.
class StreamCallGatePool::SharedGatePool : public AbstractGatePool {
 public:
  explicit SharedGatePool(std::size_t max_conns);
  ~SharedGatePool() { impl_.load(std::memory_order_acquire)->Retire(); }

  RefPtr<StreamCallGate> GetOrCreate(
      const Endpoint& key,
      FunctionView<RefPtr<StreamCallGate>()> creator) override;
  void Put(RefPtr<StreamCallGate> ptr) override;
  void Purge() override;
  void Stop() override;
  void Join() override;

 private:
  RefPtr<StreamCallGate> ConsiderReuseGate(
      std::vector<StreamCallGateEntry>* gates);

 private:
  struct Impl : HazptrObject<Impl> {
    flare::internal::HashMap<Endpoint, std::vector<StreamCallGateEntry>,
                             EndpointHash>
        gates;
  };

  std::size_t max_conns_;
  std::mutex impl_mutation_lock_;
  std::atomic<Impl*> impl_{std::make_unique<Impl>().release()};
};

// Pool for exclusive call gates.
class StreamCallGatePool::ExclusiveGatePool : public AbstractGatePool {
 public:
  RefPtr<StreamCallGate> GetOrCreate(
      const Endpoint& key,
      FunctionView<RefPtr<StreamCallGate>()> creator) override;
  void Put(RefPtr<StreamCallGate> ptr) override;
  void Purge() override;
  void Stop() override;
  void Join() override;

 private:
  std::mutex lock_;
  flare::internal::HashMap<Endpoint, std::vector<StreamCallGateEntry>,
                           EndpointHash>
      gates_;
};

// Pool for dedicated call gates.
class StreamCallGatePool::DedicatedGatePool : public AbstractGatePool {
 public:
  RefPtr<StreamCallGate> GetOrCreate(
      const Endpoint& key,
      FunctionView<RefPtr<StreamCallGate>()> creator) override {
    return creator();
  }

  void Put(RefPtr<StreamCallGate> ptr) override {
    // TODO(luobogao): Put it into a queue for destruction so as not to block
    // here.
    ptr->Stop();
    ptr->Join();
  }

  // We own nothing, so these methods are no-ops.
  void Purge() override {}
  void Stop() override {}
  void Join() override {}
};

StreamCallGatePool::SharedGatePool::SharedGatePool(std::size_t max_conns)
    : max_conns_(max_conns) {}

RefPtr<StreamCallGate> StreamCallGatePool::SharedGatePool::GetOrCreate(
    const Endpoint& key, FunctionView<RefPtr<StreamCallGate>()> creator) {
  // Let's see if we can reuse a connection first.
  do {
    Hazptr hazptr;
    auto ptr = hazptr.Keep(&impl_);
    auto iter = ptr->gates.find(key);
    if (FLARE_UNLIKELY(iter == ptr->gates.end())) {
      break;
    }
    auto&& es = iter->second;
    FLARE_CHECK_LE(es.size(), max_conns_);

    // Let's see if we can reuse a connection.
    auto gate_ptr = ConsiderReuseGate(&es);
    if (FLARE_LIKELY(gate_ptr)) {
      return gate_ptr;
    }  // Fall-through otherwise.
  } while (false);

  // Either we can or we need to create more connection then.
  auto now = ReadCoarseSteadyClock().time_since_epoch();
  new_conn_creation_in_shared_pool->Increment();

  std::scoped_lock _(impl_mutation_lock_);

  // Make a copy and add a new gate.
  auto new_impl = std::make_unique<Impl>();
  {
    Hazptr hazptr;
    new_impl->gates = hazptr.Keep(&impl_)->gates;
  }
  auto&& es = new_impl->gates[key];
  if (es.size() == max_conns_) {
    // Somebody else has already created a new one, drop our copy then.
    auto&& rc = es[Random<std::size_t>(0, max_conns_ - 1)];
    rc.last_used_since_epoch.store(now, std::memory_order_relaxed);
    return rc.gate;
  }

  // We really need to create a new one then.
  auto&& e = es.emplace_back();
  e.last_used_since_epoch.store(now, std::memory_order_relaxed);
  e.gate = creator();
  auto result = e.gate;

  impl_.exchange(new_impl.release(), std::memory_order_acq_rel)->Retire();
  return result;
}

void StreamCallGatePool::SharedGatePool::Put(RefPtr<StreamCallGate> ptr) {
  if (ptr->Healthy()) {
    // Nothing to do then. Leave the gate in the pool.
  } else {
    bool stop_and_join = false;
    {
      std::scoped_lock _(impl_mutation_lock_);

      // Make a copy of what we currently have.
      auto new_impl = std::make_unique<Impl>();
      {
        Hazptr hazptr;
        new_impl->gates = hazptr.Keep(&impl_)->gates;
      }

      // Remove the gate.
      auto&& es = new_impl->gates[ptr->GetEndpoint()];
      auto iter = std::find_if(es.begin(), es.end(), [&](auto&& e) {
        return e.gate.Get() == ptr.Get();
      });
      if (iter == es.end()) {
        // It has already been removed from the pool. Nothing then.
        stop_and_join = false;
      } else {
        es.erase(iter);
        // We're responsible for destroying it.
        //
        // TODO(luobogao): Put it into a queue for destruction so as not to
        // block here.
        stop_and_join = true;
      }

      // Update the pool.
      impl_.exchange(new_impl.release(), std::memory_order_acq_rel)->Retire();
    }
    if (stop_and_join) {
      ptr->Stop();
      ptr->Join();
    }
  }
}

void StreamCallGatePool::SharedGatePool::Purge() {
  std::vector<StreamCallGateEntry> destroying;
  {
    std::scoped_lock _(impl_mutation_lock_);
    auto new_impl = std::make_unique<Impl>();

    {
      Hazptr hazptr;
      new_impl->gates = hazptr.Keep(&impl_)->gates;
    }

    PurgeGatesUnsafe(&new_impl->gates, &destroying);
    impl_.exchange(new_impl.release(), std::memory_order_acq_rel)->Retire();
  }

  for (auto&& e : destroying) {
    e.gate->Stop();
  }
  for (auto&& e : destroying) {
    e.gate->Join();
  }
}

void StreamCallGatePool::SharedGatePool::Stop() {
  Hazptr hazptr;
  auto ptr = hazptr.Keep(&impl_);
  for (auto&& [_, es] : ptr->gates) {
    for (auto&& e : es) {
      e.gate->Stop();
    }
  }
}

void StreamCallGatePool::SharedGatePool::Join() {
  Hazptr hazptr;
  auto ptr = hazptr.Keep(&impl_);
  for (auto&& [_, es] : ptr->gates) {
    for (auto&& e : es) {
      e.gate->Join();
    }
  }
}

RefPtr<StreamCallGate> StreamCallGatePool::SharedGatePool::ConsiderReuseGate(
    std::vector<StreamCallGateEntry>* gates) {
  // We're using coarse clock here. It's important to note that this timestamp
  // does NOT change much. We're relying on this characteristic below.
  auto now = ReadCoarseSteadyClock().time_since_epoch();  // ~4ms Resolution.

  // If we're under light load, always creating up to `max_conns_` connections
  // can actually hurt performance. Linux always does a slow start once the
  // connection has been idle for some time. In our environment (RTT normally
  // falls in range of several milliseconds), this "idle period" can be as
  // small as 200ms.
  //
  // Therefore, here we take some heuristic meastures to reuse an existing
  // connection before considering creating new one.
  //
  // @sa: `net.ipv4.tcp_slow_start_after_idle`.

  // How long a connection can be idle before it's forcibly reused.
  static constexpr auto kForceReuseThreshold = 25ms;

  auto last_used = [](auto&& ptr) {
    return ptr.last_used_since_epoch.load(std::memory_order_relaxed);
  };
  auto update_timestamp_and_return = [&](auto* ptr) {
    // We don't want to keep modifying `rc.last_used_since_epoch` to reduce
    // cache traffic.
    //
    // Note that `now` is only updated periodically (see above). Therefore the
    // following condition won't hold too often (It holds each time `now` is
    // updated.).
    if (ptr->last_used_since_epoch.load(std::memory_order_relaxed) != now) {
      // This store races, but it won't hurt.
      ptr->last_used_since_epoch.store(now, std::memory_order_relaxed);
    }
    return ptr->gate;
  };

  // If 1) we've created maximum connections and *2) the last connection is used
  // soon enough*, we chose one randomly.
  //
  // The second condition is significant here. Even if we've had enough
  // connections, in case the load drops, it's possible that we no longer need
  // so many connections. Given the reuse algorithm below, the last connection's
  // timestamp should be the one farthest from now. Thus, either the load is
  // light enough that the last connection should no longer be reused, or all
  // connections are busy (when the `if`-condition holds.).
  if (gates->size() == max_conns_ &&
      last_used(gates->back()) + kForceReuseThreshold > now) {
    return update_timestamp_and_return(
        &(*gates)[Random<std::size_t>(0, max_conns_ - 1)]);
  }

  // If there's a connection that has been idle for `kForceReuseThreshold`, or
  // not currently used by more than `kMinimumUsers`, we don't bother creating a
  // new one.
  static constexpr auto kMinimumUsers = 2;  // Hmmm, let's be conservative.

  // TBH This isn't quite efficient but I don't expect `max_conns_` to be too
  // large, unless we're under heavy load, in which case there should be
  // already `max_conns_` and shouldn't be here anyway.
  for (auto&& e : *gates) {
    if (e.gate->UnsafeRefCount() < kMinimumUsers + 1 /* Ourselves. */ ||
        last_used(e) + kForceReuseThreshold < now) {
      return update_timestamp_and_return(&e);
    }
  }

  return nullptr;
}

RefPtr<StreamCallGate> StreamCallGatePool::ExclusiveGatePool::GetOrCreate(
    const Endpoint& key, FunctionView<RefPtr<StreamCallGate>()> creator) {
  std::scoped_lock _(lock_);
  auto&& entries = gates_[key];
  if (entries.empty()) {
    return creator();
  }
  auto rc = std::move(entries.back().gate);  // LIFO or FIFO?
  entries.pop_back();
  return rc;
}

void StreamCallGatePool::ExclusiveGatePool::Put(RefPtr<StreamCallGate> ptr) {
  if (!ptr->Healthy()) {
    // TODO(luobogao): Put it into a queue for destruction so as not to block
    // here.
    ptr->Stop();
    ptr->Join();
  } else {
    std::scoped_lock _(lock_);
    auto ep = ptr->GetEndpoint();
    gates_[ep].push_back(StreamCallGateEntry{
        ReadCoarseSteadyClock().time_since_epoch(), std::move(ptr)});
  }
}

void StreamCallGatePool::ExclusiveGatePool::Purge() {
  std::vector<StreamCallGateEntry> destroying;

  {
    std::scoped_lock _(lock_);
    PurgeGatesUnsafe(&gates_, &destroying);
  }
  for (auto&& e : destroying) {
    e.gate->Stop();
  }
  for (auto&& e : destroying) {
    e.gate->Join();
  }
}

void StreamCallGatePool::ExclusiveGatePool::Stop() {
  std::scoped_lock _(lock_);
  for (auto&& [_, es] : gates_) {
    for (auto&& e : es) {
      e.gate->Stop();
    }
  }
}

void StreamCallGatePool::ExclusiveGatePool::Join() {
  std::scoped_lock _(lock_);
  for (auto&& [_, es] : gates_) {
    for (auto&& e : es) {
      e.gate->Join();
    }
  }
}

StreamCallGatePool::StreamCallGatePool() {
  auto wgs = fiber::GetSchedulingGroupCount();

  shared_pools_.resize(wgs);
  for (auto&& e : shared_pools_) {
    auto max_conns = std::max<std::size_t>(
        1, FLAGS_flare_rpc_client_max_connections_per_server / wgs);
    e = std::make_unique<SharedGatePool>(max_conns);
  }

  // Gates created with `unique` flag reside here.
  shared_pools_.push_back(std::make_unique<SharedGatePool>(1));

  exclusive_pools_.resize(wgs);
  for (auto&& e : exclusive_pools_) {
    e = std::make_unique<ExclusiveGatePool>();
  }

  dedicate_pool_ = std::make_unique<DedicatedGatePool>();

  cleanup_timer_ = fiber::SetTimer(
      ReadCoarseSteadyClock(),
      FLAGS_flare_rpc_client_remove_idle_connection_interval * 1s,
      [this] { OnCleanupTimer(); });
}

void StreamCallGatePool::Stop() {
  flare::fiber::KillTimer(cleanup_timer_);
  ForEachPool([&](auto&& p) { p->Stop(); });
}

void StreamCallGatePool::Join() {
  ForEachPool([&](auto&& p) { p->Join(); });
  this_fiber::SleepFor(100ms);
  // TODO(luobogao): Wait for cleanup timer to fully stop.
}

void StreamCallGatePool::OnCleanupTimer() {
  ForEachPool([&](auto&& p) { p->Purge(); });
}

StreamCallGatePool::AbstractGatePool*
StreamCallGatePool::GetCurrentSharedGatePool(bool unique) {
  if (unique) {
    return shared_pools_.back().get();
  }
  return shared_pools_[fiber::GetCurrentSchedulingGroupIndex()].get();
}

StreamCallGatePool::AbstractGatePool*
StreamCallGatePool::GetCurrentExclusiveGatePool() {
  return exclusive_pools_[fiber::GetCurrentSchedulingGroupIndex()].get();
}

template <class F>
void StreamCallGatePool::ForEachPool(F&& op) {
  for (auto&& e : shared_pools_) {
    std::forward<F>(op)(e.get());
  }
  for (auto&& e : exclusive_pools_) {
    std::forward<F>(op)(e.get());
  }
  std::forward<F>(op)(dedicate_pool_.get());
}

StreamCallGateHandle::StreamCallGateHandle() = default;

StreamCallGateHandle::StreamCallGateHandle(
    StreamCallGatePool::AbstractGatePool* owner, RefPtr<StreamCallGate> p)
    : owner_(owner), ptr_(std::move(p)) {}

StreamCallGateHandle::~StreamCallGateHandle() { Close(); }

StreamCallGateHandle::StreamCallGateHandle(StreamCallGateHandle&&) noexcept =
    default;
StreamCallGateHandle& StreamCallGateHandle::operator=(
    StreamCallGateHandle&&) noexcept = default;

void StreamCallGateHandle::Close() noexcept {
  if (ptr_) {
    FLARE_CHECK(owner_);
    owner_->Put(std::move(ptr_));
  }
}

StreamCallGatePool* GetGlobalStreamCallGatePool(const std::string& key) {
  FLARE_CHECK(!stopped.load(std::memory_order_relaxed),
              "The call gate pool has already been stopped.");
  {
    std::shared_lock lk(call_gate_pool_lock);
    if (auto iter = call_gate_pools.find(key); iter != call_gate_pools.end()) {
      return iter->second.get();
    }
  }
  {
    std::scoped_lock lk(call_gate_pool_lock);
    if (auto iter = call_gate_pools.find(key); iter != call_gate_pools.end()) {
      return iter->second.get();
    }
    call_gate_pools[key] = std::make_unique<StreamCallGatePool>();
    return call_gate_pools[key].get();
  }
}

void StopAllGlobalStreamCallGatePools() {
  stopped = true;

  std::scoped_lock lk(call_gate_pool_lock);
  for (auto&& [k, v] : call_gate_pools) {
    v->Stop();
  }
}

void JoinAllGlobalStreamCallGatePools() {
  std::scoped_lock lk(call_gate_pool_lock);
  for (auto&& [k, v] : call_gate_pools) {
    v->Join();
  }
  call_gate_pools.clear();
}

}  // namespace flare::rpc::internal
