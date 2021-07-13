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

#ifndef FLARE_RPC_LOAD_BALANCER_LOAD_BALANCER_H_
#define FLARE_RPC_LOAD_BALANCER_LOAD_BALANCER_H_

#include <chrono>
#include <cstdint>
#include <vector>

#include "flare/base/dependency_registry.h"
#include "flare/base/net/endpoint.h"

namespace flare {

// `LoadBalancer` is responsible for selecting peer to send message.
//
// Each object is responsible for only one cluster of servers (that is,
// corresponding to one "name" resolved by `NameResolver`.).
//
// For example, we may have two `LoadBalancer`, one for selecting servers
// in "sunfish" cluster, while the other for "adx" cluster.
//
// Unless otherwise stated, THE IMPLEMENTATION MUST BE THREAD-SAFE.
class LoadBalancer {
 public:
  virtual ~LoadBalancer() = default;

  // Overwrites what we currently have.
  //
  // It's undefined to call this method concurrently (although this method can
  // be called concurrently with other methods such as `GetPeer()`) -- It simply
  // makes no sense in doing so.
  virtual void SetPeers(std::vector<Endpoint> addresses) = 0;

  virtual bool GetPeer(std::uint64_t key, Endpoint* addr,
                       std::uintptr_t* ctx) = 0;

  enum class Status {
    Success,
    Overloaded,  // Not implemented at this time.
    Failed
  };

  virtual void Report(const Endpoint& addr, Status status,
                      std::chrono::nanoseconds time_cost,
                      std::uintptr_t ctx) = 0;
};

FLARE_DECLARE_CLASS_DEPENDENCY_REGISTRY(load_balancer_registry, LoadBalancer);

}  // namespace flare

#define FLARE_RPC_REGISTER_LOAD_BALANCER(Name, Implementation)         \
  FLARE_REGISTER_CLASS_DEPENDENCY(flare::load_balancer_registry, Name, \
                                  Implementation);

#endif  // FLARE_RPC_LOAD_BALANCER_LOAD_BALANCER_H_
