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

#include "flare/rpc/message_dispatcher/composited.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace flare::message_dispatcher {

Composited::Composited(NameResolver* nr, std::unique_ptr<LoadBalancer> lb)
    : nr_(nr), lb_(std::move(lb)) {}

bool Composited::Open(const std::string& name) {
  nrv_ = nr_->StartResolving(name);
  if (!nrv_) {
    return false;
  }
  service_name_ = name;
  return true;
}

bool Composited::GetPeer(std::uint64_t key, Endpoint* addr,
                         std::uintptr_t* ctx) {
  auto version = nrv_->GetVersion();
  if (version != last_version_.load(std::memory_order_relaxed)) {
    std::scoped_lock _(reset_peers_lock_);
    if (version != last_version_.load(std::memory_order_relaxed)) {  // DCLP.
      std::vector<Endpoint> peers;
      nrv_->GetPeers(&peers);
      lb_->SetPeers(std::move(peers));
      last_version_.store(version, std::memory_order_relaxed);
    }
  }

  return lb_->GetPeer(key, addr, ctx);
}

void Composited::Report(const Endpoint& addr, Status status,
                        std::chrono::nanoseconds time_cost,
                        std::uintptr_t ctx) {
  lb_->Report(addr, status, time_cost, ctx);
}

}  // namespace flare::message_dispatcher
