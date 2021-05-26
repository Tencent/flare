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

#ifndef FLARE_RPC_INTERNAL_SHARDED_CALL_MAP_H_
#define FLARE_RPC_INTERNAL_SHARDED_CALL_MAP_H_

#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>

#include "flare/base/align.h"
#include "flare/base/internal/hash_map.h"

namespace flare::rpc::internal {

// A quick & dirty implementation of concurrent map.
template <class T>
class ShardedCallMap {
  // We use a map for each scheduling group. That means there won't be too many
  // maps. Therefore we can shard the internal map extensively.
  inline static constexpr auto kShards = 16384;

 public:
  ShardedCallMap() { shards_ = std::make_unique<Shard[]>(kShards); }

  // Insert a new correlation.
  //
  // Were duplicate found, we crash. (TODO(luobogao): Maybe we can fail more
  // gracefully.)
  void Insert(std::uint64_t correlation_id, T value) {
    auto&& shard = shards_[GetIndex(correlation_id)];
    std::scoped_lock _(shard.lock);

    auto&& [iter, inserted] =
        shard.map.emplace(correlation_id, std::move(value));
    FLARE_CHECK(inserted, "Duplicate correlation_id {}.", correlation_id);
  }

  // Returns pointer removed, or nullptr if nothing was removed.
  T Remove(std::uint64_t correlation_id) {
    auto&& shard = shards_[GetIndex(correlation_id)];
    std::scoped_lock _(shard.lock);

    if (auto iter = shard.map.find(correlation_id);
        FLARE_LIKELY(iter != shard.map.end())) {
      auto v = std::move(iter->second);
      shard.map.erase(iter);
      return v;
    }
    return nullptr;
  }

  // Call this method concurrently to other modifications may lose those
  // concurrent changes. Also note that you must not modify the map in the
  // callback. Otherwise THE BEHAVIOR IS UNDEFINED.
  template <class F>
  void ForEach(F&& f) {
    for (int i = 0; i != kShards; ++i) {
      auto&& shard = shards_[i];
      std::scoped_lock _(shard.lock);

      for (auto&& [k, v] : shard.map) {
        std::forward<F>(f)(k, v);
      }
    }
  }

 private:
  std::size_t GetIndex(std::uint64_t x) {
    // @sa: https://stackoverflow.com/a/12996028
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x % kShards;
  }

 private:
  struct alignas(hardware_destructive_interference_size) Shard {
    std::mutex lock;
    std::unordered_map<std::uint64_t, T> map;
  };
  std::unique_ptr<Shard[]> shards_;
};

}  // namespace flare::rpc::internal

#endif  // FLARE_RPC_INTERNAL_SHARDED_CALL_MAP_H_
