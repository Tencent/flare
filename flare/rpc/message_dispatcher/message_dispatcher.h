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

#ifndef FLARE_RPC_MESSAGE_DISPATCHER_MESSAGE_DISPATCHER_H_
#define FLARE_RPC_MESSAGE_DISPATCHER_MESSAGE_DISPATCHER_H_

#include <chrono>
#include <string>

#include "flare/base/dependency_registry.h"
#include "flare/base/net/endpoint.h"
#include "flare/rpc/load_balancer/load_balancer.h"

namespace flare {

// `MessageDispatcher` is responsible for choosing which server we should
// dispatch our RPC to.
//
// The `MessageDispatcher` may itself do naming resolution, or delegate it to
// some `NameResolver`.
//
// Load balancing / fault tolerance / naming resolution are all done here.
//
// Note that the implementation must be thread-safe.
class MessageDispatcher {
 public:
  virtual ~MessageDispatcher() = default;

  // Init with service name `name`.
  //
  // Format checking is done here.
  //
  // This service name is implied when `GetPeer` / `Feedback` is called.
  //
  // FIXME: Use `std::string_view` here.
  virtual bool Open(const std::string& name) = 0;

  // `key` could be used for increase the cache hit rate of the downstream
  // services (if they implemented cache, of course).
  //
  // The dispatcher tries to dispatch requests with the same `key` to the
  // same group of servers.
  //
  // The implementation is required to avoid block at their best effort.
  virtual bool GetPeer(std::uint64_t key, Endpoint* addr,
                       std::uintptr_t* ctx) = 0;

  using Status = LoadBalancer::Status;

  // For each *successful* call to `GetPeer`, there is *exactly* one
  // corresponding call to `Report`.
  virtual void Report(const Endpoint& addr, Status status,
                      std::chrono::nanoseconds time_cost,
                      std::uintptr_t ctx) = 0;
};

FLARE_DECLARE_CLASS_DEPENDENCY_REGISTRY(message_dispatcher_registry,
                                        MessageDispatcher);

}  // namespace flare

#define FLARE_RPC_REGISTER_MESSAGE_DISPATCHER(Name, Implementation)         \
  FLARE_REGISTER_CLASS_DEPENDENCY(flare::message_dispatcher_registry, Name, \
                                  Implementation);

#endif  // FLARE_RPC_MESSAGE_DISPATCHER_MESSAGE_DISPATCHER_H_
