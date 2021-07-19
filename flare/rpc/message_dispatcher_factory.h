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
//
// FIXME:Perhaps using `TypeIndex` for differentiate subsystems is better?
std::unique_ptr<MessageDispatcher> MakeMessageDispatcher(
    std::string_view subsys, std::string_view uri);

// Register a factory for a given (`subsys`, `scheme` (of `uri`)) combination.
//
// Note that `address` passed to `factory` is the `host` part of `uri`
// (https://tools.ietf.org/html/rfc3986#page-19). If you need `scheme` in
// `factory`, you should capture it on registration yourself.
//
// Factories with smaller `priority` take precedence. If `factory` does not
// recognizes `address` provided, it should return `nullptr`, and the factory
// with the next lower priority is tried.
//
// This method may only be called upon startup. (c.f. `FLARE_ON_INIT`)
void RegisterMessageDispatcherFactoryFor(
    const std::string& subsys, const std::string& scheme, int priority,
    Function<std::unique_ptr<MessageDispatcher>(std::string_view address)>
        factory);

// Register a catch-all factory for a given `subsys`. If no factory more
// specific (registered by method above) returns a non-nullptr, this factory is
// used if one is registered.
//
// If the given `scheme` or `address` is not recognized to the factory,
// `nullptr` should be returned (and the global default factory will be tried.).
//
// This method may only be called upon startup. (c.f. `FLARE_ON_INIT`)
void SetCatchAllMessageDispatcherFor(
    const std::string& subsys,
    Function<std::unique_ptr<MessageDispatcher>(std::string_view scheme,
                                                std::string_view address)>
        factory);

// This allows you to override default factory for `MakeMessageDispatcher`. The
// default factory is use when no factory (including a default one) is
// registered for a given (`subsys`, `scheme`) combination, or all factories
// registered returned `nullptr`.
//
// The behavior of the default factory is to return a `nullptr`.
//
// This method may only be called upon startup. (c.f. `FLARE_ON_INIT`)
void SetDefaultMessageDispatcherFactory(
    Function<std::unique_ptr<MessageDispatcher>(std::string_view subsys,
                                                std::string_view scheme,
                                                std::string_view address)>
        factory);

// Make a message dispatcher from the given name resolver and load balancer.
// This method is provided to ease factory implementer's life.
//
// `nullptr` is returned if either `resolver` or `load_balancer` is not
// recognized.
std::unique_ptr<MessageDispatcher> MakeCompositedMessageDispatcher(
    std::string_view resolver, std::string_view load_balancer);

}  // namespace flare

#endif  // FLARE_RPC_MESSAGE_DISPATCHER_FACTORY_H_
