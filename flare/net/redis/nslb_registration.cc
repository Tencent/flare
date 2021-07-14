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

#include "flare/base/string.h"
#include "flare/init/on_init.h"
#include "flare/rpc/message_dispatcher/message_dispatcher.h"
#include "flare/rpc/message_dispatcher_factory.h"

namespace flare::redis {

namespace {

// For illustration purpose only.
//
// std::unique_ptr<MessageDispatcher> UsingXxx(std::string_view address);

std::unique_ptr<MessageDispatcher> CatchAllUsingListRr(
    std::string_view scheme, std::string_view address) {
  return MakeCompositedMessageDispatcher("list", "rr");
}

void InitializeNslbs() {
  // For illustration purpose only.
  //
  // RegisterMessageDispatcherFactoryFor("redis", "redis", 0, UsingXxx);

  // To those who want to extend the behavior here:
  //
  // You don't need to add your registrations here to "plug-in" your NSLB logic.
  //
  // Instead, you should:
  //
  // - Write your own `cc_library`,
  // - Define a run-on-startup callback (possibly by `FLARE_ON_INIT`) and
  //   register your NSLB via `RegisterMessageDispatcherFactoryFor`.
  // - Link against your new `cc_library`.
  //
  // If desired, you can even override the "catch-all" factory below. But if you
  // want to do this, make sure you're using a lower priority than the one used
  // in `FLARE_ON_INIT`, or this on-startup callback will overwrite your
  // catch-all factory.

  SetCatchAllMessageDispatcherFor("redis", CatchAllUsingListRr);
}

}  // namespace

}  // namespace flare::redis

// Applied upon startup.
FLARE_ON_INIT(10, flare::redis::InitializeNslbs);
