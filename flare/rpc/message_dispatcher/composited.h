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

#ifndef FLARE_RPC_MESSAGE_DISPATCHER_COMPOSITED_H_
#define FLARE_RPC_MESSAGE_DISPATCHER_COMPOSITED_H_

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>

#include "flare/rpc/load_balancer/load_balancer.h"
#include "flare/rpc/message_dispatcher/message_dispatcher.h"
#include "flare/rpc/name_resolver/name_resolver.h"

namespace flare::message_dispatcher {

// A composition of `NameResolver` and `LoadBalancer`.
class Composited : public MessageDispatcher {
 public:
  Composited(NameResolver* nr, std::unique_ptr<LoadBalancer> lb);

  bool Open(const std::string& name) override;
  bool GetPeer(std::uint64_t key, Endpoint* addr, std::uintptr_t* ctx) override;
  void Report(const Endpoint& addr, Status status,
              std::chrono::nanoseconds time_cost, std::uintptr_t ctx) override;

 private:
  NameResolver* nr_;
  std::unique_ptr<NameResolutionView> nrv_;
  std::unique_ptr<LoadBalancer> lb_;
  std::string service_name_;

  std::mutex reset_peers_lock_;
  std::atomic<std::int64_t> last_version_ = NameResolutionView::kNewVersion;
};

}  // namespace flare::message_dispatcher

#endif  // FLARE_RPC_MESSAGE_DISPATCHER_COMPOSITED_H_
