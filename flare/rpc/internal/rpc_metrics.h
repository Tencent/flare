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

#ifndef FLARE_RPC_INTERNAL_RPC_METRICS_H_
#define FLARE_RPC_INTERNAL_RPC_METRICS_H_

#include <limits>
#include <memory>
#include <shared_mutex>
#include <utility>
#include <vector>

#include "jsoncpp/json.h"
#include "protobuf/descriptor.h"

#include "flare/base/internal/annotation.h"
#include "flare/base/never_destroyed.h"
#include "flare/base/write_mostly.h"
#include "flare/rpc/protocol/protobuf/rpc_meta.pb.h"

namespace flare::rpc::detail {

// Rpc requests metrics, latency metrics...
class RpcMetrics {
 private:
  typedef const google::protobuf::MethodDescriptor* MethodDescriptorPtr;

 public:
  // Register a method.
  //
  // It's not strictly necessary to call this method before calling `Report`.
  // However, without calling this method, `method` is registered lazily, and
  // you won't see your method in statistics until the first call to `Report`
  // occurs.
  void RegisterMethod(MethodDescriptorPtr method);

  // Update service method stats
  void Report(MethodDescriptorPtr method, int error_code, int elapsed_time,
              std::size_t pkt_size_in, std::size_t pkt_size_out) {
    auto&& ms = GetCached(method);
    auto&& metrics =
        (FLARE_LIKELY(error_code == STATUS_SUCCESS) ? &ms->success
                                                    : &ms->failure);
    // TODO(luobogao): Too many reports slows thing down (mostly because of
    // internal spinlock). See if we can grab a single lock and update all of
    // them.
    metrics->Report(elapsed_time);
    ms->pkt_size_in.Report(pkt_size_in);
    ms->pkt_size_out.Report(pkt_size_out);
  }

  void Dump(Json::Value* json_stat) const;

  static RpcMetrics* Instance() {
    static NeverDestroyedSingleton<RpcMetrics> mp;
    return mp.Get();
  }

 private:
  friend class NeverDestroyedSingleton<RpcMetrics>;
  RpcMetrics() {}
  RpcMetrics(const RpcMetrics&) = delete;
  RpcMetrics& operator=(const RpcMetrics&) = delete;

  struct MethodStats {
    WriteMostlyMetrics<std::uint64_t> success;
    WriteMostlyMetrics<std::uint64_t> failure;
    WriteMostlyMetrics<std::size_t> pkt_size_in;
    WriteMostlyMetrics<std::size_t> pkt_size_out;
  };

  typedef std::pair<MethodDescriptorPtr, MethodStats*> MethodCachePair;
  typedef std::pair<MethodDescriptorPtr, std::unique_ptr<MethodStats>>
      MethodPair;
  typedef std::vector<MethodCachePair> MethodCacheMap;
  typedef std::vector<MethodPair> MethodMap;

  struct CmpMethodStats {
    bool operator()(const MethodDescriptorPtr& v1,
                    const MethodDescriptorPtr& v2) const {
      return v1 < v2;
    }
    bool operator()(const MethodCachePair& v1,
                    const MethodCachePair& v2) const {
      return v1.first < v2.first;
    }
    bool operator()(const MethodPair& v1, const MethodPair& v2) const {
      return v1.first < v2.first;
    }
    bool operator()(const MethodDescriptorPtr& v1,
                    const MethodCachePair& v2) const {
      return v1 < v2.first;
    }
    bool operator()(const MethodDescriptorPtr& v1, const MethodPair& v2) const {
      return v1 < v2.first;
    }
    bool operator()(const MethodPair& v1, const MethodDescriptorPtr& v2) const {
      return v1.first < v2;
    }
    bool operator()(const MethodCachePair& v1,
                    const MethodDescriptorPtr& v2) const {
      return v1.first < v2;
    }
  };

  Json::Value DumpMethodStats(const MethodStats* method_stats) const;

  MethodStats* GetCached(MethodDescriptorPtr method) {
    // Hack an element so that we don't need to check vector.end() for
    // lower_bound.
    FLARE_INTERNAL_TLS_MODEL thread_local MethodCacheMap cache{
        MethodCachePair(reinterpret_cast<MethodDescriptorPtr>(
                            std::numeric_limits<std::uintptr_t>::max()),
                        nullptr)};
    auto&& it =
        std::lower_bound(cache.begin(), cache.end(), method, CmpMethodStats());
    if (FLARE_UNLIKELY(it->first != method)) {
      RegisterMethod(method, &cache, &it);
    }
    return it->second;
  }

  void RegisterMethod(MethodDescriptorPtr method, MethodCacheMap* cache,
                      MethodCacheMap::iterator* cache_it);

  // Protects method_map_.
  mutable std::shared_mutex lock_;
  MethodMap method_map_;
  MethodStats ms;
};

}  // namespace flare::rpc::detail

#endif  // FLARE_RPC_INTERNAL_RPC_METRICS_H_
