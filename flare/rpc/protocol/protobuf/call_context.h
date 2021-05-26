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

#ifndef FLARE_RPC_PROTOCOL_PROTOBUF_CALL_CONTEXT_H_
#define FLARE_RPC_PROTOCOL_PROTOBUF_CALL_CONTEXT_H_

#include <limits>

#include "thirdparty/protobuf/message.h"

#include "flare/base/maybe_owning.h"
#include "flare/base/object_pool.h"
#include "flare/rpc/protocol/controller.h"

namespace flare::protobuf {

// This call context is used when we're proactively making calls. That is, it's
// used at client side.
struct ProactiveCallContext : Controller {
  // Set if we're holding a response prototype (as opposed of a response
  // buffer.).
  bool expecting_stream;
  // Set if the response should not be deserialized by the framework.
  bool accept_response_in_bytes;
  google::protobuf::Message* response_ptr = nullptr;
  const google::protobuf::Message* response_prototype = nullptr;

  // Method being called.
  const google::protobuf::MethodDescriptor* method;

  // Return `response_ptr` or create a new response buffer from
  // `response_prototype`, depending on whether `expecting_stream` is set.
  MaybeOwning<google::protobuf::Message> GetOrCreateResponse();

  ProactiveCallContext() { SetRuntimeTypeTo<ProactiveCallContext>(); }
};

// This context is used when we're called passively, i.e., at server side.
struct PassiveCallContext : Controller {
  PassiveCallContext() { SetRuntimeTypeTo<PassiveCallContext>(); }

  // Not everyone set this. See implementation of the protocol object for
  // detail.
  const google::protobuf::MethodDescriptor* method = nullptr;

  // Used solely by trpc protocol.
  std::uint32_t trpc_content_type;
};

}  // namespace flare::protobuf

namespace flare {

template <>
struct PoolTraits<protobuf::ProactiveCallContext> {
  static constexpr auto kType = PoolType::MemoryNodeShared;
  static constexpr auto kLowWaterMark = 8192;
  static constexpr auto kHighWaterMark =
      std::numeric_limits<std::size_t>::max();
  static constexpr auto kMaxIdle = std::chrono::seconds(10);
  static constexpr auto kMinimumThreadCacheSize = 1024;
  // 100 transfers per second for 1M QPS.
  static constexpr auto kTransferBatchSize = 1024;

  static void OnGet(protobuf::ProactiveCallContext* p) {
    p->response_ptr = nullptr;
    p->response_prototype = nullptr;
  }
};

}  // namespace flare

#endif  // FLARE_RPC_PROTOCOL_PROTOBUF_CALL_CONTEXT_H_
