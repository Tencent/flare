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

#include "flare/rpc/load_balancer/round_robin.h"

#include <memory>
#include <utility>

namespace flare::load_balancer {

FLARE_RPC_REGISTER_LOAD_BALANCER("rr", RoundRobin);

RoundRobin::~RoundRobin() {
  endpoints_.load()->Retire();  // Let it go.
}

void RoundRobin::SetPeers(std::vector<Endpoint> addresses) {
  auto new_peers = std::make_unique<Peers>();
  new_peers->peers = std::move(addresses);
  endpoints_.exchange(new_peers.release(), std::memory_order_acq_rel)->Retire();
}

bool RoundRobin::GetPeer(std::uint64_t key, Endpoint* addr,
                         std::uintptr_t* ctx) {
  Hazptr hazptr;
  auto kept = hazptr.Keep(&endpoints_);

  if (FLARE_UNLIKELY(kept->peers.empty())) {
    return false;
  }
  *addr = kept->peers[next_.fetch_add(1, std::memory_order_relaxed) %
                      kept->peers.size()];
  return true;
}

void RoundRobin::Report(const Endpoint& addr, Status status,
                        std::chrono::nanoseconds time_cost,
                        std::uintptr_t ctx) {
  // TODO(luobogao): ...
  // CHECK(status == Status::Success) << "Not implemented: Removing bad peers.";
}

}  // namespace flare::load_balancer
