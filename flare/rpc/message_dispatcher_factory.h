// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef FLARE_RPC_MESSAGE_DISPATCHER_FACTORY_H_
#define FLARE_RPC_MESSAGE_DISPATCHER_FACTORY_H_

#include <memory>
#include <string>
#include <string_view>

#include "flare/base/function.h"
#include "flare/base/internal/macro.h"
#include "flare/rpc/message_dispatcher/message_dispatcher.h"

namespace flare {

// Create a new message dispatcher for subsystem `subsys`. `uri`, together with
// `subsys`, is used to determine which NSLB should be used.
//
// `subsys` is defined by users (e.g., `RpcChannel`) of this method.
//
// It's still the caller's responsibility to call `Open` on the resulting
// message dispatcher.
std::unique_ptr<MessageDispatcher> MakeMessageDispatcher(
    std::string_view subsys, std::string_view uri);

// Register a factory for a given (`subsys`, `scheme` (of `uri` above))
// combination.
//
// Factories with smaller `priority` take precedence. If `factory` does not
// recognizes `uri` provided, it should return `nullptr`, and the factory with
// the next lower priority is tried.
//
// This method may only be called upon startup.
void RegisterMessageDispatcherFactoryFor(
    const std::string& subsys, const std::string& scheme, int priority,
    Function<std::unique_ptr<MessageDispatcher>(std::string_view uri)> factory);

// This allows you to override default factory for `MakeMessageDispatcher`. The
// default factory is use when no factory is registered for a given (`subsys`,
// `scheme`) combination, or all factories registered returned `nullptr`.
//
// This method may only be called upon startup.
void SetDefaultMessageDispatcherFactory(
    Function<std::unique_ptr<MessageDispatcher>(std::string_view subsys,
                                                std::string_view uri)>
        factory);

// Make a message dispatcher from the given name resolver and load balancer.
// This method is provided to ease factory implementer's life.
std::unique_ptr<MessageDispatcher> MakeCompositedMessageDispatcher(
    std::string_view resolver, std::string_view load_balancer);

}  // namespace flare

// Register message dispatcher factory for the given `Subsystem` and `Scheme`.
// Lower priority takes precedence.
//
// Multiple `Factory`-ies may be registered for the same scheme. If `Factory`
// does not recognize `uri` given to it, `nullptr` should be returned. In this
// case the next lower priority `Factory` is tried (and so on.).
#define FLARE_RPC_REGISTER_MESSAGE_DISPATCHER_FACTORY_FOR(Subsystem, Scheme,  \
                                                          Priority, Factory)  \
  [[gnu::constructor]] static void FLARE_INTERNAL_PP_CAT(                     \
      flare_reserved_registry_message_dispatcher_for_, __COUNTER__)() {       \
    ::flare::RegisterMessageDispatcherFactoryFor(Subsystem, Scheme, Priority, \
                                                 Factory);                    \
  }

#endif  // FLARE_RPC_MESSAGE_DISPATCHER_FACTORY_H_
