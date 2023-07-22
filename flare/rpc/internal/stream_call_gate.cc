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

#include "flare/rpc/internal/stream_call_gate.h"

#include <chrono>
#include <limits>
#include <utility>
#include <vector>

#include "gflags/gflags.h"

#include "flare/base/internal/early_init.h"
#include "flare/base/logging.h"
#include "flare/fiber/fiber.h"
#include "flare/fiber/runtime.h"
#include "flare/fiber/this_fiber.h"
#include "flare/fiber/timer.h"
#include "flare/io/event_loop.h"
#include "flare/io/native/stream_connection.h"
#include "flare/io/util/socket.h"

DEFINE_int32(
    flare_rpc_client_stream_concurrency, 2,
    "Maximum number of messages that is being or waiting for processing. "
    "Specifying a number too small may degrade overall performance if "
    "streaming rpcs and normal rpcs are performed on same connection.");

namespace flare {

template <>
struct PoolTraits<rpc::internal::StreamCallGate::FastCallContext> {
  static constexpr auto kType = PoolType::MemoryNodeShared;
  static constexpr auto kLowWaterMark = 8192;
  static constexpr auto kHighWaterMark =
      std::numeric_limits<std::size_t>::max();
  static constexpr auto kMaxIdle = std::chrono::seconds(10);
  static constexpr auto kMinimumThreadCacheSize = 4096;
  static constexpr auto kTransferBatchSize = 1024;

  static void OnPut(rpc::internal::StreamCallGate::FastCallContext* ptr) {
    FLARE_CHECK(ptr->user_args == nullptr);
  }
};

namespace rpc::internal {

StreamCallGate::StreamCallGate() = default;

StreamCallGate::~StreamCallGate() = default;

void StreamCallGate::Open(const Endpoint& address, Options options) {
  options_ = std::move(options);
  endpoint_ = address;
  correlation_map_ = GetCorrelationMapFor<PooledPtr<FastCallContext>>(
      fiber::GetCurrentSchedulingGroupIndex());

  FLARE_CHECK(options_.protocol);

  if (!InitializeConnection(address)) {
    SetUnhealthy();
  }
}

bool StreamCallGate::Healthy() const {
  return healthy_.load(std::memory_order_relaxed);
}

void StreamCallGate::SetUnhealthy() {
  healthy_.store(false, std::memory_order_relaxed);
}

void StreamCallGate::Stop() {
  UnsafeRaiseErrorGlobally();
  if (conn_) {
    conn_->Stop();
  }
  {
    std::scoped_lock _(stream_ctxs_lock_);
    for (auto&& [cid, ctx] : stream_ctxs_) {
      ctx->adaptor->Break();
    }
  }
}

void StreamCallGate::Join() {
  if (conn_) {
    conn_->Join();
  }

  // Wait until all stream are closed.
  while (true) {
    std::scoped_lock _(stream_ctxs_lock_);
    if (stream_ctxs_.empty()) {
      break;
    }
    this_fiber::Yield();
  }

  // Wait until the streams are reaped.
  if (stream_reaper_) {
    stream_reaper_->Stop();
    stream_reaper_->Join();
  }
}

const Endpoint& StreamCallGate::GetEndpoint() const { return endpoint_; }

const StreamProtocol& StreamCallGate::GetProtocol() const {
  return *options_.protocol;
}

void StreamCallGate::FastCall(const Message& m, PooledPtr<FastCallArgs> args,
                              std::chrono::steady_clock::time_point timeout) {
  FLARE_CHECK_LE(m.GetCorrelationId(),
                 std::numeric_limits<std::uint32_t>::max(),
                 "Unsupported: 64-bit RPC correlation ID.");

  // Serialization is done prior to fill `Timestamps.sent_tsc`.
  auto serialized = WriteMessage(m, args->controller);

  {
    // This lock guarantees us that no one else races with us.
    //
    // The race is unlikely but possible:
    //
    // 1. We allocated the context for this packet.
    // 2. Before we finished initializing this context (and before we send this
    //    packet out), the remote side (possibly erroneously) sends us a packet
    //    with the same correlation ID this packet carries, and triggers
    //    incoming packet callback.
    // 3. The callback removes the timeout timer.
    //
    // In this case, we'd risk use-after-free when enabling the timeout timer
    // later.
    std::unique_lock<fiber::Mutex> ctx_lock;
    std::uint64_t timeout_timer = 0;

    if (timeout != std::chrono::steady_clock::time_point::max()) {
      auto timeout_cb = [map = correlation_map_,
                         conn_cid = conn_correlation_id_,
                         rpc_cid = m.GetCorrelationId()](auto) mutable {
        RaiseErrorIfPresentFastCall(map, conn_cid, rpc_cid,
                                    CompletionStatus::Timeout);
      };
      timeout_timer =
          fiber::internal::CreateTimer(timeout, std::move(timeout_cb));
    }

    // Initialize call context in call map.
    AllocateRpcContextFastCall(m.GetCorrelationId(), [&](FastCallContext* ctx) {
      ctx_lock = std::unique_lock(ctx->lock);

      ctx->timestamps.sent_tsc = ReadTsc();  // Not exactly.
      ctx->timeout_timer = timeout_timer;
      ctx->user_args = std::move(args);
    });
    if (timeout_timer) {
      fiber::internal::EnableTimer(timeout_timer);
    }
  }

  // Raise an error early if the connection is not healthy.
  if (!healthy_.load(std::memory_order_acquire)) {
    RaiseErrorIfPresentFastCall(correlation_map_, conn_correlation_id_,
                                m.GetCorrelationId(),
                                CompletionStatus::IoError);
  } else {
    WriteOut(std::move(serialized), 0 /* ctx */);
  }
}

PooledPtr<StreamCallGate::FastCallArgs> StreamCallGate::CancelFastCall(
    std::uint32_t correlation_id) {
  auto ptr = TryReclaimRpcContextFastCall(correlation_id);
  return ptr ? std::move(ptr->user_args) : nullptr;
}

std::pair<AsyncStreamReader<StreamCallGate::MessagePtr>,
          AsyncStreamWriter<StreamCallGate::MessagePtr>>
StreamCallGate::StreamCall(std::uint64_t correlation_id,
                           Controller* controller) {
  std::call_once(stream_reaper_init_, [&] { stream_reaper_.Init(); });

  auto try_parse = [this, correlation_id](auto&& e) {
    // This is dirty.
    //
    // However, we need to ensure `controller` has not been destroyed when using
    // it.
    //
    // The reason is subtle. Closing a stream is completed asynchronously at the
    // moment. That is, it can completes even before all pending messages are
    // fully parsed (in the stream's work queue.). We cannot flush the work
    // queue when `stream.Close()` is called though, since that method itself
    // can be called in the work queue, and waiting there can effectively leads
    // to deadlock.
    //
    // The other way around can be after parsing a message in the work queue,
    // fire yet another fiber for calling user's code. This guarantee us that
    // flushing work queue when `stream.Close()` is called won't deadlock.
    //
    // I'll take a deeper look later.
    auto rc =
        LockRpcContextStreamIfPresent(correlation_id, [&](StreamContext* sctx) {
          return options_.protocol->TryParse(e, sctx->controller);
        });
    return rc /* `correlation_id` is found. */ &&
           *rc /* `TryParse` succeeded. */;
  };
  rpc::detail::StreamIoAdaptor::Operations ops = {
      .try_parse = try_parse,
      .write =
          [=, this](auto&& e) {
            // `controller` must be alive. `write` is called synchronously from
            // `stream.Write()`. If the user destroyed the controller before
            // writing, he should be quite aware what he's doing.
            return WriteOut(WriteMessage(e, controller), correlation_id);
          },
      .restart_read = [this] { conn_->RestartRead(); },
      .on_close = [this,
                   correlation_id] { return OnStreamClosed(correlation_id); },
      .on_cleanup =
          [this, correlation_id] { return OnStreamCleanup(correlation_id); }};
  auto adaptor = std::make_unique<rpc::detail::StreamIoAdaptor>(
      FLAGS_flare_rpc_client_stream_concurrency, std::move(ops));
  auto adp = adaptor.get();

  AllocateRpcContextStream(correlation_id, [&](StreamContext& ctx) {
    ctx.controller = controller;
    ctx.adaptor = std::move(adaptor);
  });
  if (!healthy_.load(std::memory_order_acquire)) {
    RaiseErrorIfPresentStreamCall(correlation_id, CompletionStatus::IoError);
  }
  // TODO(luobogao): Pass execution context to `StreamIoAdaptor`.
  return std::pair(std::move(adp->GetStreamReader()),
                   std::move(adp->GetStreamWriter()));
}

bool StreamCallGate::InitializeConnection(const Endpoint& ep) {
  // Initialize socket.
  auto fd = io::util::CreateStreamSocket(ep.Family());
  if (!fd) {
    FLARE_LOG_ERROR_EVERY_SECOND("Failed to create socket with AF {}.",
                                 ep.Family());
    return false;
  }
  io::util::SetCloseOnExec(fd.Get());
  io::util::SetNonBlocking(fd.Get());
  io::util::SetTcpNoDelay(fd.Get());
  // `io::util::SetSendBufferSize` & `io::util::SetReceiveBufferSize`?
  if (!io::util::StartConnect(fd.Get(), ep)) {
    FLARE_LOG_WARNING_EVERY_SECOND("Failed to connection to [{}].",
                                   ep.ToString());
    return false;
  }

  // Initialize connection.
  NativeStreamConnection::Options opts;
  opts.handler =
      MaybeOwning(non_owning, static_cast<StreamConnectionHandler*>(this));
  opts.read_buffer_size = options_.maximum_packet_size;
  conn_ =
      MakeRefCounted<NativeStreamConnection>(std::move(fd), std::move(opts));

  // Add the connection to event loop.
  event_loop_ =
      GetGlobalEventLoop(fiber::GetCurrentSchedulingGroupIndex(), conn_->fd());
  event_loop_->AttachDescriptor(conn_.Get(), false);

  // `conn_`'s callbacks may access `conn_` itself, so we must delay enabling
  // the descriptor until `conn_` is also initialized.
  event_loop_->EnableDescriptor(conn_.Get());

  conn_->StartHandshaking();

  return true;
}

bool StreamCallGate::WriteOut(NoncontiguousBuffer buffer, std::uintptr_t ctx) {
  return conn_->Write(std::move(buffer), ctx);
}

// Allocate a context associated with `correlation_id`. `f` is called to
// initialize the context.
template <class F>
void StreamCallGate::AllocateRpcContextFastCall(std::uint32_t correlation_id,
                                                F&& init) {
  auto ptr = object_pool::Get<FastCallContext>();
  std::forward<F>(init)(&*ptr);

  correlation_map_->Insert(
      MergeCorrelationId(conn_correlation_id_, correlation_id), std::move(ptr));
}

// Reclaim rpc context if `correlation_id` is associated with a fast call,
// otherwise `nullptr` is returned.
//
// Note that this method also returns `nullptr` if `correlation_id` does not
// exist at all. This may somewhat degrade performance of processing
// streams, we might optimize it some day later.
PooledPtr<StreamCallGate::FastCallContext>
StreamCallGate::TryReclaimRpcContextFastCall(std::uint32_t correlation_id) {
  return correlation_map_->Remove(
      MergeCorrelationId(conn_correlation_id_, correlation_id));
}

// Traverse in-use fast-call correlations.
template <class F>
void StreamCallGate::ForEachRpcContextFastCall(F&& f) {
  correlation_map_->ForEach([&](auto key, auto&& v) {
    auto&& [conn, rpc] = SplitCorrelationId(key);
    if (conn == conn_correlation_id_) {
      f(rpc, v);
    }
  });
}

template <class F>
void StreamCallGate::AllocateRpcContextStream(std::uint64_t correlation_id,
                                              F&& init) {
  // We only check for non-zero correlation ID for stream calls. For fast calls
  // we don't want to check this to support protocols who do not support
  // multiplexing.
  FLARE_CHECK(correlation_id != 0,
              "`0` is not a valid correlation ID for streaming RPC. Use a "
              "positive integer instead.");

  auto ptr = std::make_unique<StreamContext>();
  init(*ptr);
  std::scoped_lock lk(stream_ctxs_lock_);
  FLARE_CHECK(stream_ctxs_.find(correlation_id) == stream_ctxs_.end(),
              "Duplicate correlation ID {}.", correlation_id);
  stream_ctxs_[correlation_id] = std::move(ptr);
}

void StreamCallGate::DisableRpcContextStream(std::uint64_t correlation_id) {
  std::scoped_lock lk(stream_ctxs_lock_);
  auto iter = stream_ctxs_.find(correlation_id);
  FLARE_CHECK(iter != stream_ctxs_.end());
  iter->second->closed = true;
}

template <class F, class FR, class R>
R StreamCallGate::LockRpcContextStreamIfPresent(std::uint64_t correlation_id,
                                                F&& f) {
  // `stream_ctxs_lock_` is a fiber mutex. This is required as `f` may cause
  // underlying pthread worker to change, which is not acceptable for pthread
  // mutex.
  std::scoped_lock lk(stream_ctxs_lock_);
  auto iter = stream_ctxs_.find(correlation_id);

  if (iter == stream_ctxs_.end() || iter->second->closed) {
    if constexpr (std::is_void_v<FR>) {
      return false;
    } else {
      return std::nullopt;
    }
  }
  if constexpr (std::is_void_v<FR>) {
    std::forward<F>(f)(iter->second.get());
    return true;
  } else {
    return std::forward<F>(f)(iter->second.get());
  }
}

template <class F>
void StreamCallGate::ReclaimRpcContextStream(std::uint64_t correlation_id,
                                             F&& cb) {
  std::scoped_lock lk(stream_ctxs_lock_);
  auto iter = stream_ctxs_.find(correlation_id);

  FLARE_CHECK(iter != stream_ctxs_.end());
  auto rc = std::move(iter->second);
  stream_ctxs_.erase(iter);
  FLARE_CHECK(rc);
  std::forward<F>(cb)(std::move(rc));
}

void StreamCallGate::OnStreamClosed(std::uint64_t correlation_id) {
  DisableRpcContextStream(correlation_id);
}

void StreamCallGate::OnStreamCleanup(std::uint64_t correlation_id) {
  ReclaimRpcContextStream(
      correlation_id, [&](std::unique_ptr<StreamContext> ctx) {
        stream_reaper_->Push(
            [ctx = std::move(ctx)] { ctx->adaptor->FlushPendingCalls(); });
      });
}

EventLoop* StreamCallGate::GetEventLoop() { return event_loop_; }

// Called in dedicated fiber. Blocking is OK.
void StreamCallGate::ServiceFastCallCompletion(std::unique_ptr<Message> msg,
                                               PooledPtr<FastCallContext> ctx,
                                               std::uint64_t tsc) {
  ctx->timestamps.received_tsc = tsc;
  {
    // Wait until the context is fully initialized (if not yet).
    std::scoped_lock _(ctx->lock);
  }

  if (auto t = std::exchange(ctx->timeout_timer, 0)) {  // We set a timer.
    fiber::internal::KillTimer(t);
  }

  auto user_args = std::move(ctx->user_args);
  auto cb = [&] {
    auto ctlr = user_args->controller;
    auto f = std::move(user_args->completion);
    std::unique_ptr<Message> parsed =
        options_.protocol->TryParse(&msg, ctlr) ? std::move(msg) : nullptr;

    ctx->timestamps.parsed_tsc = ReadTsc();
    if (parsed) {
      f(CompletionStatus::Success, std::move(parsed), ctx->timestamps);
    } else {
      f(CompletionStatus::ParseError, nullptr, ctx->timestamps);
    }
  };

  // Respect the caller's execution context if it's there.
  if (user_args->exec_ctx) {
    user_args->exec_ctx->Execute(cb);
  } else {
    cb();
  }

  // `*this` may not be touched since now as user's callback might have already
  // freed us.
}

void StreamCallGate::OnDataWritten(std::uintptr_t ctx) {
  if (ctx) {  // Associated with a stream.
    // Correlation ID was passed as `ctx` to `StreamConnection::Write`.
    LockRpcContextStreamIfPresent(ctx, [&](StreamContext* ctx) {
      ctx->adaptor->NotifyWriteCompletion();
    });
  }
}

StreamConnectionHandler::DataConsumptionStatus StreamCallGate::OnDataArrival(
    NoncontiguousBuffer* buffer) {
  auto arrival_tsc = ReadTsc();
  bool ever_suppressed = false;
  while (!buffer->Empty()) {
    std::unique_ptr<Message> m;
    auto rc = options_.protocol->TryCutMessage(buffer, &m);

    if (rc == StreamProtocol::MessageCutStatus::ProtocolMismatch ||
        rc == StreamProtocol::MessageCutStatus::Error) {
      FLARE_LOG_WARNING_EVERY_SECOND(
          "Failed to cut message off from connection to [{}]. Closing.",
          endpoint_.ToString());
      return DataConsumptionStatus::Error;
    } else if (rc == StreamProtocol::MessageCutStatus::NotIdentified ||
               rc == StreamProtocol::MessageCutStatus::NeedMore) {
      return !ever_suppressed ? DataConsumptionStatus::Ready
                              : DataConsumptionStatus::SuppressRead;
    }
    FLARE_CHECK(rc == StreamProtocol::MessageCutStatus::Cut);

    // Dispatch the message.
    auto correlation_id = m->GetCorrelationId();

    // TODO(luobogao): We could infer the message type (fast call or stream) by
    // examining `message->GetType()` and move the rest into dedicated fiber.
    // However, that way we'd have a hard time in removing the timeout timer.
    // We might have to refactor timer's interface to resolve this.
    if (auto ctx = TryReclaimRpcContextFastCall(correlation_id)) {
      // It's a fast call then.
      if (FLARE_UNLIKELY(!options_.protocol->GetCharacteristics()
                              .ignore_message_type_for_client_side_streaming &&
                         m->GetType() != Message::Type::Single)) {
        FLARE_LOG_WARNING_EVERY_SECOND(
            "Message #{} is marked as part of a stream, but we're expecting a "
            "normal RPC response.",
            m->GetCorrelationId());
        continue;
      }
      // FIXME: We need to wait for the callback to return before we could be
      // destroyed.
      fiber::internal::StartFiberDetached([this, tsc = arrival_tsc,
                                           msg = std::move(m),
                                           ctx = std::move(ctx)]() mutable {
        ServiceFastCallCompletion(std::move(msg), std::move(ctx), tsc);
      });
    } else {
      // Let's see if it belongs to a stream.
      auto op = LockRpcContextStreamIfPresent(
          correlation_id, [&](StreamContext* ctx) {
            if (ctx->eos_seen) {
              FLARE_LOG_WARNING_EVERY_SECOND(
                  "Received message from call {} after EOS is seen.",
                  correlation_id);
              return false;
            }
            auto type = m->GetType();  // `m` is moved away soon.
            auto rc = ctx->adaptor->NotifyRead(std::move(m));
            // For multiple-request-single-response scenario, the response is
            // marked as `Single`.
            if (!options_.protocol->GetCharacteristics()
                     .ignore_message_type_for_client_side_streaming &&
                !!(type & Message::Type::EndOfStream)) {
              ctx->eos_seen = true;
              ctx->adaptor->NotifyError(StreamError::EndOfStream);
            }
            return rc;
          });
      if (!op) {  // There's no stream associated with the correlation id.
        FLARE_VLOG(10, "No context for call {} is found. Message dropped.",
                   correlation_id);
      } else if (*op) {  // Internal buffer full.
        // Note that in this case we *must* consume all remaining data before
        // suppressing further read. Otherwise if the remote side close the
        // connection before we re-start reading, we will lose those data.
        ever_suppressed = true;
      }  // Nothing special otherwise.
    }
  }  // Loop until no more message could be cut off.
  return !ever_suppressed ? DataConsumptionStatus::Ready
                          : DataConsumptionStatus::SuppressRead;
}

void StreamCallGate::OnAttach(StreamConnection*) {}
void StreamCallGate::OnDetach() {}
void StreamCallGate::OnWriteBufferEmpty() {}
void StreamCallGate::OnClose() { OnError(); }
void StreamCallGate::OnError() {
  healthy_.store(false, std::memory_order_release);
  UnsafeRaiseErrorGlobally();
}

NoncontiguousBuffer StreamCallGate::WriteMessage(const Message& message,
                                                 Controller* controller) const {
  NoncontiguousBuffer serialized;

  options_.protocol->WriteMessage(message, &serialized, controller);
  return serialized;
}

// CAUTION: THIS METHOD CAN BE CALLED EITHER FROM PTHREAD CONTEXT (ON TIMEOUT)
// OR FIBER CONTEXT (ON IO ERROR.).
void StreamCallGate::RaiseErrorIfPresentFastCall(
    CorrelationMap<PooledPtr<FastCallContext>>* map,
    std::uint32_t conn_correlation_id, std::uint32_t rpc_correlation_id,
    CompletionStatus status) {
  auto ctx =
      map->Remove(MergeCorrelationId(conn_correlation_id, rpc_correlation_id));

  if (!ctx) {  // Completed in the mean time?
    return;    // Nothing then.
  }

  fiber::internal::StartFiberDetached([ctx = std::move(ctx), status]() mutable {
    {
      // Make sure the context is fully initialized.
      std::scoped_lock _(ctx->lock);
    }
    if (auto t = std::exchange(ctx->timeout_timer, 0)) {
      fiber::internal::KillTimer(t);
    }
    auto user_args = std::move(ctx->user_args);
    FLARE_CHECK(!!user_args->completion);
    auto f = std::move(user_args->completion);
    if (user_args->exec_ctx) {
      user_args->exec_ctx->Execute([&] {
        // TODO(luobogao): Pass `error` to the callback..
        f(status, nullptr, flare::internal::EarlyInitConstant<Timestamps>());
      });
    } else {
      f(status, nullptr, flare::internal::EarlyInitConstant<Timestamps>());
    }
  });
}

void StreamCallGate::RaiseErrorIfPresentStreamCall(std::uint64_t correlation_id,
                                                   CompletionStatus status) {
  LockRpcContextStreamIfPresent(correlation_id, [&](StreamContext* ctx) {
    if (ctx->eos_seen) {
      return;
    } else {
      ctx->eos_seen = true;
    }
    if (status == CompletionStatus::IoError) {
      ctx->adaptor->NotifyError(StreamError::IoError);
    } else if (status == CompletionStatus::Timeout) {
      ctx->adaptor->NotifyError(StreamError::Timeout);
    } else {
      FLARE_CHECK(0, "Unexpected error #{}.", underlying_value(status));
    }
  });
}

void StreamCallGate::UnsafeRaiseErrorGlobally() {
  std::vector<std::uint64_t> stream_cids;

  {
    std::scoped_lock lk(stream_ctxs_lock_);
    for (auto&& [x, _] : stream_ctxs_) {
      stream_cids.push_back(x);
    }
  }
  for (auto&& e : stream_cids) {
    RaiseErrorIfPresentStreamCall(e, CompletionStatus::IoError);
  }

  std::vector<std::uint64_t> fast_cids;
  ForEachRpcContextFastCall(
      [&](auto&& cid, auto&& ctx) { fast_cids.push_back(cid); });

  for (auto&& c : fast_cids) {
    RaiseErrorIfPresentFastCall(correlation_map_, conn_correlation_id_, c,
                                CompletionStatus::IoError);
  }
}

}  // namespace rpc::internal

}  // namespace flare
