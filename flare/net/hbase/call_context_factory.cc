// Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "flare/net/hbase/call_context_factory.h"

#include <memory>

#include "flare/base/chrono.h"
#include "flare/net/hbase/call_context.h"
#include "flare/net/hbase/hbase_server_controller.h"

namespace flare::hbase {

PassiveCallContextFactory passive_call_context_factory;

std::unique_ptr<Controller> PassiveCallContextFactory::Create(
    bool streaming_call) const {
  FLARE_CHECK(!streaming_call,
              "Unexpected: HBase protocol does not support streaming RPC, but "
              "the protocol object did recognized a streaming call.");
  auto ctx = std::make_unique<PassiveCallContext>();
  return ctx;
}

}  // namespace flare::hbase
