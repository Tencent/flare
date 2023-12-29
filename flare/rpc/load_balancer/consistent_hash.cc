// Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "flare/rpc/load_balancer/consistent_hash.h"
#include "flare/base/string.h"

namespace flare::load_balancer {

FLARE_RPC_REGISTER_LOAD_BALANCER("chash", ConsistentHash);

ConsistentHash::~ConsistentHash() { endpoints_.load()->Retire(); }

void ConsistentHash::SetPeers(std::vector<Endpoint> addresses) {
  std::map<std::uint64_t, Endpoint> peers_circle;
  for (auto&& addr : addresses) {
    // Take the same weight for each Endpoint
    for (std::uint64_t i = 0; i < kVirtualNodePeerEndpoint; i++) {
      std::uint64_t key = hash_(Format("{},{}", addr, i));
      peers_circle.emplace(key, std::move(addr));
    }
  }

  auto new_peers = std::make_unique<Peers>();
  new_peers->peers_circle_ = std::move(peers_circle);
  endpoints_.exchange(new_peers.release(), std::memory_order_acq_rel)->Retire();
}

bool ConsistentHash::GetPeer(std::uint64_t key, Endpoint* addr,
                             std::uintptr_t* ctx) {
  Hazptr hazptr;
  auto kept = hazptr.Keep(&endpoints_);

  if (FLARE_UNLIKELY(kept->peers_circle_.empty())) {
    return false;
  }
  auto itr = kept->peers_circle_.lower_bound(hash_(std::to_string(key)));
  *addr = itr == kept->peers_circle_.end() ? kept->peers_circle_.begin()->second
                                           : itr->second;
  return true;
}

void ConsistentHash::Report(const Endpoint& addr, Status status,
                            std::chrono::nanoseconds time_cost,
                            std::uintptr_t ctx) {
  // TODO(yinghaoyu): ...
}
}  // namespace flare::load_balancer
