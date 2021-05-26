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

#ifndef FLARE_RPC_LOAD_BALANCER_ROUND_ROBIN_H_
#define FLARE_RPC_LOAD_BALANCER_ROUND_ROBIN_H_

#include <atomic>
#include <memory>
#include <vector>

#include "flare/base/hazptr.h"
#include "flare/base/random.h"
#include "flare/rpc/load_balancer/load_balancer.h"

namespace flare::load_balancer {

// A "real" load balancer. Only responsible for load balancing, has nothing
// to do about name resolving.
class RoundRobin : public LoadBalancer {
 public:
  ~RoundRobin();

  void SetPeers(std::vector<Endpoint> addresses) override;

  // `key` is ignored, as we select endpoints in a round-robin fashion.
  bool GetPeer(std::uint64_t key, Endpoint* addr, std::uintptr_t* ctx) override;

  void Report(const Endpoint& addr, Status status,
              std::chrono::nanoseconds time_cost, std::uintptr_t ctx) override;

 private:
  struct Peers : HazptrObject<Peers> {
    std::vector<Endpoint> peers;
  };

  std::atomic<std::size_t> next_{Random()};  // FIXME: Make it thread-local.
  std::atomic<Peers*> endpoints_{std::make_unique<Peers>().release()};
};

}  // namespace flare::load_balancer

#endif  // FLARE_RPC_LOAD_BALANCER_ROUND_ROBIN_H_
