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

#ifndef FLARE_RPC_INTERNAL_STREAM_CALL_GATE_POOL_H_
#define FLARE_RPC_INTERNAL_STREAM_CALL_GATE_POOL_H_

#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gflags/gflags_declare.h"

#include "flare/base/function_view.h"
#include "flare/base/internal/early_init.h"
#include "flare/base/net/endpoint.h"
#include "flare/base/ref_ptr.h"
#include "flare/fiber/timer.h"

DECLARE_int32(flare_rpc_client_max_connections_per_server);
DECLARE_int32(flare_rpc_client_connection_max_idle);

namespace flare::rpc::internal {

class StreamCallGate;
class StreamCallGateHandle;

// This pool is responsible for gates management. `Channel`s should not create
// gates themselves, instead, `StreamCallGatePool` should be used.
//
// There's a pool for each protocol.
class StreamCallGatePool {
 public:
  StreamCallGatePool();

  // This method tries to find an existing call gate based on `key`, if none is
  // found, `creator` is called to create a new one.
  //
  // If `unique` is specified, only one gate will be created for a given `key`.
  // Otherwise the implementation might create dedicated gates for each NUMA
  // domains if it sees appropriate. This option is not supported if
  // `shared_gate` is not specified.
  //
  // If a new gate is created, it will be kept alive by the pool until
  // `max_idle` for potential reuse in the future. This argument is ignored if
  // an existing call gate is returned. Note that this argument is not strictly
  // respected, it may get extended by several seconds due to implementation
  // limitations.
  //
  // `creator` should return a `RefPtr<StreamCallGate>`.
  template <class F>
  StreamCallGateHandle GetOrCreateShared(const Endpoint& key, bool unique,
                                         F&& creator);

  // This method also tries to pool the gates, but it will only return gates not
  // used by others. This is needed by HTTP 1.1 and several other protocols.
  template <class F>
  StreamCallGateHandle GetOrCreateExclusive(const Endpoint& key, F&& creator);

  // The gate is not pooled at all. Gate created by `creator` is immediately
  // passed back to the caller in this case.
  //
  // Our old-fashioned streaming RPC need this.
  template <class F>
  StreamCallGateHandle GetOrCreateDedicated(F&& creator);

  void Stop();
  void Join();

 private:
  void OnCleanupTimer();

 private:
  friend class StreamCallGateHandle;

  class AbstractGatePool {
   public:
    virtual ~AbstractGatePool() = default;

    // Get a gate from pool, if there's no eligible one, create a new gate.
    virtual RefPtr<StreamCallGate> GetOrCreate(
        const Endpoint& key,
        FunctionView<RefPtr<StreamCallGate>()> creator) = 0;

    // In case the gate is in error state, the implementation is responsible for
    // stopping & removing it from the pool.
    virtual void Put(RefPtr<StreamCallGate> ptr) = 0;

    // Called periodically. The implementation is responsible for removing gates
    // that have been idle for a while.
    virtual void Purge() = 0;

    // For shutting down the pool.
    virtual void Stop() = 0;
    virtual void Join() = 0;
  };

  class SharedGatePool;
  class ExclusiveGatePool;
  class DedicatedGatePool;

  // Get gate pool associated with current worker group, or the dedicated unique
  // pool if specified.
  AbstractGatePool* GetCurrentSharedGatePool(bool unique);
  AbstractGatePool* GetCurrentExclusiveGatePool();

  // Helper method for execute operations on all the pools.
  template <class F>
  void ForEachPool(F&& op);

 private:
  // One for each worker group, plus one for "unique" gates.
  std::vector<std::unique_ptr<AbstractGatePool>> shared_pools_;

  // Exclusive gates are kept separately.
  //
  // One for each worker group. We do not support "unique" exclusive gates.
  std::vector<std::unique_ptr<AbstractGatePool>> exclusive_pools_;

  // There's No Such Thing as a "dedicated pool". The gates are always created
  // on request, and destroyed when RPC completes. We create one here for the
  // sake of simplicity of implementation.
  std::unique_ptr<AbstractGatePool> dedicate_pool_;

  // This timer periodically calls `Purge` on all the pools.
  std::uint64_t cleanup_timer_;
};

// RAII wrapper for `StreamCallGate*`.
class StreamCallGateHandle {
 public:
  StreamCallGateHandle();

  // Note that you should only call `release` if the gate is still usable
  // (`Healthy()` holds), otherwise you should free the gate instead of return
  // it back to the pool.
  StreamCallGateHandle(StreamCallGatePool::AbstractGatePool* owner,
                       RefPtr<StreamCallGate> p);
  ~StreamCallGateHandle();

  // Move-able only.
  StreamCallGateHandle(StreamCallGateHandle&&) noexcept;
  StreamCallGateHandle& operator=(StreamCallGateHandle&&) noexcept;
  StreamCallGateHandle(const StreamCallGateHandle&) = delete;
  StreamCallGateHandle& operator=(const StreamCallGateHandle&) = delete;

  // Accessor.
  StreamCallGate* Get() const { return ptr_.Get(); }
  StreamCallGate* operator->() const { return Get(); }
  StreamCallGate& operator*() const { return *Get(); }
  explicit operator bool() const noexcept { return !!Get(); }

  void Close() noexcept;

 private:
  StreamCallGatePool::AbstractGatePool* owner_;
  RefPtr<StreamCallGate> ptr_;
};

// We use a dedicated pool for each `key`. Usually protocol name is used as
// `key`. However, if connection can't be shared between different
// configurations (e.g., when credential is associated with connections, such as
// Redis), you may want to add extra components into `key`.
StreamCallGatePool* GetGlobalStreamCallGatePool(const std::string& key);

void StopAllGlobalStreamCallGatePools();
void JoinAllGlobalStreamCallGatePools();

// Implementations go below.

template <class F>
StreamCallGateHandle StreamCallGatePool::GetOrCreateShared(const Endpoint& key,
                                                           bool unique,
                                                           F&& creator) {
  auto&& pool = GetCurrentSharedGatePool(unique);
  auto rc = pool->GetOrCreate(key, std::forward<F>(creator));
  FLARE_CHECK(rc->GetEndpoint() == key);
  return StreamCallGateHandle(pool, std::move(rc));
}

template <class F>
StreamCallGateHandle StreamCallGatePool::GetOrCreateExclusive(
    const Endpoint& key, F&& creator) {
  auto&& pool = GetCurrentExclusiveGatePool();
  auto rc = pool->GetOrCreate(key, std::forward<F>(creator));
  FLARE_CHECK(rc->GetEndpoint() == key);
  return StreamCallGateHandle(pool, std::move(rc));
}

template <class F>
StreamCallGateHandle StreamCallGatePool::GetOrCreateDedicated(F&& creator) {
  auto&& pool = dedicate_pool_.get();
  return StreamCallGateHandle(
      pool, pool->GetOrCreate(flare::internal::EarlyInitConstant<Endpoint>(),
                              std::forward<F>(creator)));
}

}  // namespace flare::rpc::internal

#endif  // FLARE_RPC_INTERNAL_STREAM_CALL_GATE_POOL_H_
