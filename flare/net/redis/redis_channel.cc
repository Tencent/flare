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

#include "flare/net/redis/redis_channel.h"

#include <chrono>
#include <cinttypes>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "flare/base/casting.h"
#include "flare/base/encoding/hex.h"
#include "flare/base/string.h"
#include "flare/net/redis/message.h"
#include "flare/net/redis/mock_channel.h"
#include "flare/net/redis/redis_protocol.h"
#include "flare/rpc/internal/stream_call_gate.h"
#include "flare/rpc/internal/stream_call_gate_pool.h"
#include "flare/rpc/message_dispatcher_factory.h"

using namespace std::literals;
using flare::rpc::internal::StreamCallGate;

namespace flare {

redis::detail::MockChannel* mock_channel = nullptr;

namespace {

RedisError TranslateError(StreamCallGate::CompletionStatus status) {
  FLARE_CHECK(status != StreamCallGate::CompletionStatus::Success);
  if (status == StreamCallGate::CompletionStatus::IoError) {
    return RedisError{"X-IO", "I/O error."};
  } else if (status == StreamCallGate::CompletionStatus::Timeout) {
    return RedisError{"X-TIMEOUT", "Redis request timeout."};
  }
  return RedisError{"X-UNKNOWN", "Unknown error."};
}

}  // namespace

struct RedisChannel::Impl {
  bool opened = false;
  bool using_mock_channel = false;
  std::unique_ptr<MessageDispatcher> msg_dispatcher;
  rpc::internal::StreamCallGatePool* call_gate_pool;
};

RedisChannel::RedisChannel() { impl_ = std::make_unique<Impl>(); }

RedisChannel::~RedisChannel() = default;

RedisChannel::RedisChannel(const std::string& uri, const Options& options)
    : RedisChannel() {
  (void)Open(uri, options);
}

bool RedisChannel::Open(const std::string& address, const Options& options) {
  options_ = options;

  if (FLARE_UNLIKELY(StartsWith(address, "mock://"))) {
    impl_->using_mock_channel = true;
    impl_->opened = true;
    return true;
  }

  static constexpr auto kUriPrefix = "redis://"sv;
  FLARE_CHECK(StartsWith(address, kUriPrefix));

  auto rest = std::string_view(address).substr(kUriPrefix.size());
  impl_->msg_dispatcher = MakeMessageDispatcher("redis", address);
  if (!impl_->msg_dispatcher ||
      !impl_->msg_dispatcher->Open(std::string(rest))) {
    FLARE_LOG_WARNING_EVERY_SECOND("Failed to open Redis cluster [{}].",
                                   address);
    return false;
  }
  // To avoid possible ambiguity introduced by colon in username / password, we
  // encode them before building "call gate pool" key.
  impl_->call_gate_pool = rpc::internal::GetGlobalStreamCallGatePool(
      Format("redis:{}:{})", EncodeHex(options.username),
             EncodeHex(options.password)));

  impl_->opened = true;
  return true;
}

void RedisChannel::RegisterMockChannel(redis::detail::MockChannel* channel) {
  mock_channel = channel;
}

void RedisChannel::Execute(const RedisCommand& command,
                           Function<void(RedisObject&&)> cb,
                           std::chrono::steady_clock::time_point timeout) {
  // Was the channel opened successfully?
  if (FLARE_UNLIKELY(!impl_->opened)) {
    cb(RedisObject(
        RedisError{"X-NOT-OPENED", "Channel has not been opened yet."}));
    return;
  }

  if (FLARE_UNLIKELY(impl_->using_mock_channel)) {
    FLARE_CHECK(mock_channel,
                "Redis mock channel has not been registered yet. Forget to "
                "link `//flare/testing:redis_mock?");

    return mock_channel->Execute(nullptr, &command, &cb, timeout);
  }

  // Choose a peer to contact.
  Endpoint peer;
  std::uintptr_t msg_disp_ctx;
  if (!impl_->msg_dispatcher->GetPeer(0, &peer, &msg_disp_ctx)) {
    cb(RedisObject(
        RedisError{"X-CONN", "Failed to determine Redis peer to connect."}));
    return;
  }

  // Make CKV call.
  auto handle = CreateCallGate(peer);
  RefPtr gate_ref(ref_ptr, handle.Get());
  auto internal_cb = [handle = std::move(handle), cb = std::move(cb)](
                         StreamCallGate::CompletionStatus status,
                         std::unique_ptr<Message> msg,
                         const StreamCallGate::Timestamps& ts) {
    if (status == StreamCallGate::CompletionStatus::Success) {
      FLARE_CHECK(msg);
      cb(std::move(cast<redis::RedisResponse>(*msg)->object));
    } else {
      handle->SetUnhealthy();  // Must be destroyed as it's not multiplexable.
      cb(TranslateError(status));
    }
  };
  auto call_args = object_pool::Get<StreamCallGate::FastCallArgs>();
  call_args->completion = std::move(internal_cb);
  call_args->controller = nullptr;
  if (auto ptr = fiber::ExecutionContext::Current()) {
    call_args->exec_ctx.Reset(ref_ptr, ptr);
  }

  redis::RedisRequest msg;
  msg.command = &command;
  gate_ref->FastCall(msg, std::move(call_args), timeout);
}

rpc::internal::StreamCallGateHandle RedisChannel::CreateCallGate(
    const Endpoint& endpoint) {
  auto cb = [&]() {
    auto protocol = std::make_unique<redis::RedisProtocol>();
    protocol->SetCredential(options_.username, options_.password);

    auto gate = MakeRefCounted<StreamCallGate>();
    StreamCallGate::Options opts;
    opts.protocol = std::move(protocol);
    opts.maximum_packet_size = options_.maximum_packet_size;
    gate->Open(endpoint, std::move(opts));
    if (!gate->Healthy()) {
      FLARE_LOG_WARNING_EVERY_SECOND("Failed to connect to Redis server [{}].",
                                     endpoint.ToString());
      // Fall-through. Error will be raised when making RPC.
    }
    return gate;
  };
  return impl_->call_gate_pool->GetOrCreateExclusive(endpoint, cb);
}

}  // namespace flare
