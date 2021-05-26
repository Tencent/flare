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

#include "flare/rpc/internal/normal_connection_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "opentracing/ext/tags.h"

#include "flare/base/deferred.h"
#include "flare/base/internal/dpc.h"
#include "flare/base/tsc.h"
#include "flare/fiber/future.h"
#include "flare/fiber/latch.h"
#include "flare/fiber/this_fiber.h"
#include "flare/rpc/binlog/dumper.h"
#include "flare/rpc/internal/session_context.h"
#include "flare/rpc/server.h"
#include "flare/rpc/tracing/framework_tags.h"
#include "flare/rpc/tracing/tracing_ops.h"

using namespace std::literals;

DECLARE_int32(flare_rpc_server_stream_concurrency);

DEFINE_bool(
    flare_rpc_start_new_trace_on_missing, false,
    "If set, the framework will start a new RPC trace if no one is available "
    "yet. For programs accepting incoming requests, the caller should have "
    "already passed a tracing context along with its request, in this case the "
    "caller's trace is extended (instead of starting a new one).");

namespace flare::rpc::detail {

NormalConnectionHandler::NormalConnectionHandler(Server* owner,
                                                 std::unique_ptr<Context> ctx)
    : owner_(owner), ctx_(std::move(ctx)) {
  FLARE_CHECK(!ctx_->services.empty(),
              "No service is enabled, confused about what to serve.");

  last_service_ = ctx_->services.front();
}

void NormalConnectionHandler::Stop() {
  // Break all ongoing streams.
  std::scoped_lock lk(lock_);
  for (auto&& [cid, sctx] : streams_) {
    sctx.io_adaptor->Break();
  }
}

void NormalConnectionHandler::Join() {
  while (ongoing_requests_.load(std::memory_order_acquire)) {
    this_fiber::SleepFor(100ms);
  }

  // Wait until all stream's `OnStreamClosed()` are called.
  while (true) {
    {
      std::scoped_lock lk(lock_);
      if (streams_.empty()) {
        break;
      }
    }
    this_fiber::SleepFor(100ms);
  }

  // Any finally wait for them being reaped.
  if (stream_reaper_) {
    stream_reaper_->Stop();
    stream_reaper_->Join();
  }
}

void NormalConnectionHandler::OnAttach(StreamConnection* conn) { conn_ = conn; }

void NormalConnectionHandler::OnDetach() {}

void NormalConnectionHandler::OnWriteBufferEmpty() {}

StreamConnectionHandler::DataConsumptionStatus
NormalConnectionHandler::OnDataArrival(NoncontiguousBuffer* buffer) {
  FLARE_CHECK(conn_);  // This cannot fail.

  ScopedDeferred _([&] { ConsiderUpdateCoarseLastEventTimestamp(); });
  bool ever_suppressed = false;
  auto receive_tsc = ReadTsc();

  while (!buffer->Empty()) {
    auto rc = ProcessOnePacket(buffer, receive_tsc);
    if (FLARE_LIKELY(rc == ProcessingStatus::Success)) {
      continue;
    } else if (rc == ProcessingStatus::Error) {
      return DataConsumptionStatus::Error;
    } else if (rc == ProcessingStatus::SuppressRead) {
      ever_suppressed = true;
    } else {
      FLARE_CHECK(rc == ProcessingStatus::Saturated);
      return ever_suppressed ? DataConsumptionStatus::SuppressRead
                             : DataConsumptionStatus::Ready;
    }
  }
  return ever_suppressed ? DataConsumptionStatus::SuppressRead
                         : DataConsumptionStatus::Ready;
}

void NormalConnectionHandler::OnClose() {
  FLARE_VLOG(10, "Connection from [{}] closed.", ctx_->remote_peer.ToString());

  owner_->OnConnectionClosed(ctx_->id);
}

void NormalConnectionHandler::OnError() {
  FLARE_VLOG(10, "Error on connection from [{}].",
             ctx_->remote_peer.ToString());
  OnClose();
}

void NormalConnectionHandler::OnDataWritten(std::uintptr_t ctx) {
  if (ctx != kFastCallReservedContextId) {
    // Stream call.
    std::scoped_lock lk(lock_);
    if (auto iter = streams_.find(ctx); iter != streams_.end()) {
      iter->second.io_adaptor->NotifyWriteCompletion();
    } else {
      FLARE_VLOG(
          10,
          "Response to stream #{} was successfully written, but the stream "
          "itself has gone.",
          ctx);
    }
  }
}

NormalConnectionHandler::ProcessingStatus
NormalConnectionHandler::ProcessOnePacket(NoncontiguousBuffer* buffer,
                                          std::uint64_t receive_tsc) {
  auto buffer_size_was = buffer->ByteSize();
  std::unique_ptr<Message> msg;
  StreamProtocol* protocol;

  // Cut one message off without parsing it, so as to minimize blocking I/O
  // fiber.
  auto rc = TryCutMessage(buffer, &msg, &protocol);
  if (rc == ProcessingStatus::Error) {
    FLARE_LOG_ERROR_EVERY_SECOND("Unrecognized packet from [{}]. ",
                                 ctx_->remote_peer.ToString());
    return ProcessingStatus::Error;
  } else if (rc == ProcessingStatus::Saturated) {
    return ProcessingStatus::Saturated;
  }
  FLARE_CHECK(rc == ProcessingStatus::Success);
  auto pkt_size = buffer_size_was - buffer->ByteSize();  // Size of this packet.

  if (auto type = msg->GetType(); FLARE_LIKELY(type == Message::Type::Single)) {
    // Call service to handle it in separate fiber.
    //
    // FIXME: We always create start a fiber in the background to call service,
    // this is needed for better responsiveness in I/O fiber (had we used
    // `Dispatch` here, I/O fiber will be keep migrating between workers).
    // However, in this case, responsiveness of RPCs suffers. It might be better
    // if we call last service in foreground.

    // This check must be done here.
    //
    // Should we increment the in-fly RPC counter (via `OnNewCall`) in separate
    // fiber, there's no appropriate time point for us to wait for the counter
    // to monotonically decrease (since it can increase at any time, depending
    // on when that separate fiber is scheduled.).
    //
    // By increment the counter in I/O fiber, we can simply execute a
    // `Barrier()` on the event loop after we `Stop()`-ped the connection.
    if (FLARE_UNLIKELY(!OnNewCall())) {
      // This can't be done in dedicated fiber (for now). Since we didn't
      // increase ongoing request counter, were we run this method in dedicated
      // fiber, it's possible that `*this` is destroyed before the fiber is
      // finished.
      auto ctlr = NewController(*msg, protocol);
      ServiceOverloaded(std::move(msg), protocol, ctlr.get());
    } else {
      // FIXME: Too many captures hurts performance.
      fiber::internal::StartFiberDetached([this, msg = std::move(msg), protocol,
                                           receive_tsc, pkt_size]() mutable {
        auto ctlr = NewController(*msg, protocol);
        ServiceFastCall(std::move(msg), protocol, std::move(ctlr), receive_tsc,
                        pkt_size);
        OnCallCompletion();
      });
    }
    return ProcessingStatus::Success;
  } else {
    std::scoped_lock lk(lock_);
    auto correlation_id = msg->GetCorrelationId();
    auto iter = streams_.find(correlation_id);

    if (correlation_id == kFastCallReservedContextId) {
      FLARE_LOG_ERROR_EVERY_SECOND(
          "Unsupported correlation_id [{}] in stream call. Dropped.",
          kFastCallReservedContextId);
      return ProcessingStatus::Success;
    }

    // Well we don't check for `StartOfStream` here. Indeed doing some basic
    // sanity checks would be great but some protocol (notably QZone) does not
    // support start-of-stream / end-of-stream indicator. Besides, given that
    // our transport protocol (e.g., TCP) itself already guarantees reliable
    // packet delivery, there's not much point in checking for this (except for
    // programming errors.).
    bool is_new_stream = (iter == streams_.end());
    std::shared_ptr<Controller> ctlr;

    // Initialize the stream.
    if (is_new_stream) {
      ctlr = NewController(*msg, protocol);

      if (!OnNewCall()) {  // Must be done in I/O fiber. See above.
        ServiceOverloaded(std::move(msg), protocol, ctlr.get());
        return ProcessingStatus::Success;
      }

      InitializeStreamContextLocked(correlation_id, protocol, ctlr);
      iter = streams_.find(correlation_id);
      FLARE_CHECK(iter != streams_.end());
    }

    // Won't block, won't call user's code.
    auto rc = iter->second.io_adaptor->NotifyRead(std::move(msg))
                  ? ProcessingStatus::SuppressRead
                  : ProcessingStatus::Success;
    if (!!(type == Message::Type::EndOfStream)) {
      iter->second.io_adaptor->NotifyError(StreamError::EndOfStream);
    }

    if (is_new_stream) {
      // This is a new stream, tell the service about the fact.
      // `ServiceStreamCall` is called in separate fiber.
      fiber::internal::StartFiberDetached([this, correlation_id, protocol,
                                           ctlr = std::move(ctlr), receive_tsc,
                                           pkt_size]() mutable {
        ServiceStreamCall(correlation_id, protocol, std::move(ctlr),
                          receive_tsc, pkt_size);
        OnCallCompletion();
      });
    }
    return rc;
  }
}

StreamProtocol::MessageCutStatus
NormalConnectionHandler::TryCutMessageUsingLastProtocol(
    NoncontiguousBuffer* buffer, std::unique_ptr<Message>* msg,
    StreamProtocol** used_protocol) {
  FLARE_CHECK(ever_succeeded_cut_msg_);
  FLARE_CHECK_LT(last_protocol_, ctx_->protocols.size());
  auto&& last_prot = ctx_->protocols[last_protocol_];
  auto rc = last_prot->TryCutMessage(buffer, msg);
  if (FLARE_LIKELY(rc == StreamProtocol::MessageCutStatus::Cut)) {
    *used_protocol = last_prot.get();
    return StreamProtocol::MessageCutStatus::Cut;
  } else if (rc == StreamProtocol::MessageCutStatus::NotIdentified ||
             rc == StreamProtocol::MessageCutStatus::NeedMore) {
    return StreamProtocol::MessageCutStatus::NeedMore;
  } else if (rc == StreamProtocol::MessageCutStatus::ProtocolMismatch) {
    return StreamProtocol::MessageCutStatus::ProtocolMismatch;
  } else if (rc == StreamProtocol::MessageCutStatus::Error) {
    return StreamProtocol::MessageCutStatus::Error;
  }
  FLARE_CHECK(0, "Unexpected status {}.", underlying_value(rc));
}

NormalConnectionHandler::ProcessingStatus
NormalConnectionHandler::TryCutMessage(NoncontiguousBuffer* buffer,
                                       std::unique_ptr<Message>* msg,
                                       StreamProtocol** used_protocol) {
  // If we succeeded in cutting off a message, we try the protocol we last used
  // first. Normally the protocol shouldn't change.
  if (FLARE_LIKELY(ever_succeeded_cut_msg_)) {
    auto rc = TryCutMessageUsingLastProtocol(buffer, msg, used_protocol);
    if (rc == StreamProtocol::MessageCutStatus::Cut) {
      return ProcessingStatus::Success;
    } else if (rc == StreamProtocol::MessageCutStatus::Error) {
      return ProcessingStatus::Error;
    } else if (rc == StreamProtocol::MessageCutStatus::NeedMore) {
      return ProcessingStatus::Saturated;
    } else {
      FLARE_CHECK(rc == StreamProtocol::MessageCutStatus::ProtocolMismatch,
                  "Unexpected status: {}.", underlying_value(rc));
      // Fallback to detect the protocol then.
    }
  }

  bool ever_need_more = false;

  // By reaching here, we have no idea which protocol is `buffer` using, so we
  // try all of the protocols.
  for (std::size_t index = 0; index != ctx_->protocols.size(); ++index) {
    auto&& protocol = ctx_->protocols[index];
    auto rc = protocol->TryCutMessage(buffer, msg);
    if (rc == StreamProtocol::MessageCutStatus::Cut) {
      *used_protocol = protocol.get();
      ever_succeeded_cut_msg_ = true;
      last_protocol_ = index;
      return ProcessingStatus::Success;
    } else if (rc == StreamProtocol::MessageCutStatus::NeedMore) {
      return ProcessingStatus::Saturated;
    } else if (rc == StreamProtocol::MessageCutStatus::Error) {
      return ProcessingStatus::Error;
    } else if (rc == StreamProtocol::MessageCutStatus::NotIdentified) {
      ever_need_more = true;
      continue;
    } else {
      FLARE_CHECK(rc == StreamProtocol::MessageCutStatus::ProtocolMismatch,
                  "Unexpected status: {}.", underlying_value(rc));
      // Loop then.
    }
  }

  // If at least one protocol need more bytes to investigate further, we tell
  // the caller this fact, otherwise an error is returned.
  return ever_need_more ? ProcessingStatus::Saturated : ProcessingStatus::Error;
}

std::unique_ptr<Controller> NormalConnectionHandler::NewController(
    const Message& message, StreamProtocol* protocol) const {
  return protocol->GetControllerFactory()->Create(message.GetType() !=
                                                  Message::Type::Single);
}

void NormalConnectionHandler::WriteOverloaded(const Message& corresponding_req,
                                              StreamProtocol* protocol,
                                              Controller* controller) {
  auto stream = corresponding_req.GetType() != Message::Type::Single;
  auto factory = protocol->GetMessageFactory();
  auto msg = factory->Create(MessageFactory::Type::Overloaded,
                             corresponding_req.GetCorrelationId(), stream);
  if (msg) {  // Note that `MessageFactory::Create` may return `nullptr`.
    WriteMessage(*msg, protocol, controller, kFastCallReservedContextId);
  }
}

std::size_t NormalConnectionHandler::WriteMessage(const Message& msg,
                                                  StreamProtocol* protocol,
                                                  Controller* controller,
                                                  std::uintptr_t ctx) const {
  ScopedDeferred _([&] { ConsiderUpdateCoarseLastEventTimestamp(); });
  NoncontiguousBuffer nb;
  protocol->WriteMessage(msg, &nb, controller);
  auto bytes = nb.ByteSize();
  (void)conn_->Write(std::move(nb), ctx);  // Failure is ignored.
  return bytes;
}

void NormalConnectionHandler::ServiceFastCall(
    std::unique_ptr<Message>&& msg, StreamProtocol* protocol,
    std::unique_ptr<Controller> controller, std::uint64_t receive_tsc,
    std::size_t pkt_size) {
  auto dispatched_tsc = ReadTsc();

  // If the request has been in queue for too long, reject it now.
  if (FLARE_UNLIKELY(ctx_->max_request_queueing_delay != 0ns &&
                     DurationFromTsc(receive_tsc, dispatched_tsc) >
                         ctx_->max_request_queueing_delay)) {
    FLARE_LOG_WARNING_EVERY_SECOND(
        "Request #{} has been in queue for too long, rejected.",
        msg->GetCorrelationId());
    WriteOverloaded(*msg, protocol, &*controller);
    return;
  }

  // Parse the packet first.
  auto cid = msg->GetCorrelationId();
  if (FLARE_UNLIKELY(!protocol->TryParse(&msg, controller.get()))) {
    FLARE_LOG_WARNING_EVERY_SECOND("Failed to parse message #{}.", cid);
    return;
  }
  auto parsed_tsc = ReadTsc();
  // It was, and it should still be.
  FLARE_CHECK(msg->GetType() == Message::Type::Single);

  // Call context we'll pass to the handler.
  StreamService::Context call_context;
  call_context.incoming_packet_size = pkt_size;
  call_context.local_peer = ctx_->local_peer;
  call_context.remote_peer = ctx_->remote_peer;
  call_context.received_tsc = receive_tsc;
  call_context.dispatched_tsc = dispatched_tsc;
  call_context.parsed_tsc = parsed_tsc;
  call_context.controller = controller.get();

  // Let's see who would be able to handle this message.
  StreamService::InspectionResult inspection_result;
  auto handler =
      FindAndCacheMessageHandler(*msg, *controller, &inspection_result);
  if (FLARE_UNLIKELY(!handler)) {  // No one is interested in this message. (But
                                   // why would any protocol produced it then?)
    FLARE_LOG_ERROR_EVERY_SECOND(
        "Received a message of type [{}] from [{}] which is not interested by "
        "any service. The message was successfully parsed by protocol [{}].",
        GetTypeName(*msg), ctx_->remote_peer.ToString(),
        protocol->GetCharacteristics().name);
    return;
  }

  // Prepare the execution context and call user's code.
  PrepareForRpc(inspection_result, *controller, [&] {
    InitializeForTracing(inspection_result, *controller);
    InitializeForDumpingBinlog(inspection_result, &call_context);

    // Call user's code.
    auto writer = [&](auto&& m) {
      return WriteMessage(m, protocol, controller.get(),
                          kFastCallReservedContextId);
    };

    auto processing_status = handler->FastCall(&msg, writer, &call_context);
    if (processing_status == StreamService::ProcessingStatus::Processed ||
        processing_status == StreamService::ProcessingStatus::Completed) {
      // Nothing.
    } else {
      call_context.status = -1;  // ...
      if (processing_status == StreamService::ProcessingStatus::Overloaded) {
        WriteOverloaded(*msg, protocol, &*controller);
      }  // No special action required for other errors.
    }

    WaitForRpcCompletion();

    FinishDumpingBinlog(handler->GetUuid(), inspection_result, call_context);
    FinishTracing(controller.get(), call_context);

    // The connection should be closed ASAP. Calling `OnConnectionClosed` looks
    // weird as we're actually actively closing the connection. Anyway, that
    // callback should serve us well.
    if (processing_status == StreamService::ProcessingStatus::Completed) {
      owner_->OnConnectionClosed(ctx_->id);
    }
  });
}

void NormalConnectionHandler::InitializeStreamContextLocked(
    std::uint64_t correlation_id, StreamProtocol* protocol,
    std::shared_ptr<Controller> controller) {
  std::call_once(stream_reaper_init_, [&] { stream_reaper_.Init(); });

  FLARE_CHECK(streams_.find(correlation_id) == streams_.end());
  auto&& ctx = streams_[correlation_id];
  auto pctlr = controller.get();

  StreamIoAdaptor::Operations ops = {
      .try_parse = [=](auto&& e) { return protocol->TryParse(e, pctlr); },
      .write =
          [=](auto&& am) {
            return WriteMessage(am, protocol, pctlr, correlation_id);
          },
      .restart_read = [=] { return conn_->RestartRead(); },
      .on_close = [=] { return OnStreamClosed(correlation_id); },
      .on_cleanup = [=] { return OnStreamCleanup(correlation_id); }};
  ctx.io_adaptor = std::make_unique<StreamIoAdaptor>(
      FLAGS_flare_rpc_server_stream_concurrency, std::move(ops));
}

void NormalConnectionHandler::ServiceStreamCall(
    std::uint64_t correlation_id, StreamProtocol* protocol,
    std::shared_ptr<Controller> controller, std::uint64_t receive_tsc,
    std::size_t pkt_size) {
  auto now = ReadTsc();

  StreamService::Context call_context;
  call_context.incoming_packet_size = pkt_size;
  call_context.local_peer = ctx_->local_peer;
  call_context.remote_peer = ctx_->remote_peer;
  call_context.controller = controller.get();
  call_context.streaming_call_no_eos_marker =
      protocol->GetCharacteristics().no_end_of_stream_marker;
  call_context.received_tsc = receive_tsc;
  call_context.dispatched_tsc = now;
  call_context.parsed_tsc = now;  // Not accurate, it's parsed on read.

  StreamContext* sctx;
  {
    std::scoped_lock lk(lock_);
    auto iter = streams_.find(correlation_id);
    FLARE_CHECK(iter != streams_.end(),
                "Call {} is missing. It can't be since there shouldn't be "
                "anyone else aware of this call.",
                correlation_id);
    sctx = &iter->second;
  }

  auto&& stream_reader = std::move(sctx->io_adaptor->GetStreamReader());
  auto&& stream_writer = std::move(sctx->io_adaptor->GetStreamWriter());

  Deferred stream_closer([&] {
    fiber::BlockingGet(stream_reader.Close());
    fiber::BlockingGet(stream_writer.Close());
  });

  auto* first_msg =
      fiber::BlockingGet(sctx->io_adaptor->GetStreamReader().Peek())
          ->value()
          .get();

  // If the request has been in queue for too long, reject it now.
  if (FLARE_UNLIKELY(ctx_->max_request_queueing_delay != 0ns &&
                     DurationFromTsc(receive_tsc, ReadTsc()) >
                         ctx_->max_request_queueing_delay)) {
    FLARE_LOG_WARNING_EVERY_SECOND(
        "Request #{} has been in queue for too long, rejected.",
        first_msg->GetCorrelationId());
    WriteOverloaded(*first_msg, protocol, &*controller);
    return;
  }

  StreamService::InspectionResult inspection_result;
  auto handler =
      FindAndCacheMessageHandler(*first_msg, *controller, &inspection_result);
  if (FLARE_UNLIKELY(!handler)) {
    FLARE_LOG_WARNING_EVERY_SECOND("Unrecognized stream from [{}].",
                                   ctx_->remote_peer.ToString());
    return;
  }

  PrepareForRpc(inspection_result, *controller, [&] {
    InitializeForTracing(inspection_result, *controller);
    InitializeForDumpingBinlog(inspection_result, &call_context);

    auto processing_status =
        handler->StreamCall(&stream_reader, &stream_writer, &call_context);
    if (processing_status == StreamService::ProcessingStatus::Processed ||
        processing_status == StreamService::ProcessingStatus::Completed) {
      stream_closer.Dismiss();  // Both stream should be closed by the service.
    } else {
      call_context.status = -1;  // ...
      if (processing_status == StreamService::ProcessingStatus::Overloaded) {
        WriteOverloaded(*first_msg, protocol, &*controller);
      }  // We don't take special action for other errors.
    }

    WaitForRpcCompletion();

    FinishDumpingBinlog(handler->GetUuid(), inspection_result, call_context);
    FinishTracing(controller.get(), call_context);

    if (processing_status == StreamService::ProcessingStatus::Completed) {
      owner_->OnConnectionClosed(ctx_->id);
    }
  });
}

void NormalConnectionHandler::ServiceOverloaded(std::unique_ptr<Message> msg,
                                                StreamProtocol* protocol,
                                                Controller* controller) {
  FLARE_LOG_WARNING_EVERY_SECOND("Server overloaded. Message is dropped.");

  WriteOverloaded(*msg, protocol, controller);
}

inline StreamService* NormalConnectionHandler::FindAndCacheMessageHandler(
    const Message& message, const Controller& controller,
    StreamService::InspectionResult* inspection_result) {
  auto last = last_service_.load(std::memory_order_relaxed);
  if (FLARE_LIKELY(last->Inspect(message, controller, inspection_result))) {
    return last;
  }
  return FindAndCacheMessageHandlerSlow(
      message, controller, inspection_result);  // Update `last_service_`.
}

StreamService* NormalConnectionHandler::FindAndCacheMessageHandlerSlow(
    const Message& message, const Controller& controller,
    StreamService::InspectionResult* inspection_result) {
  for (auto&& e : ctx_->services) {
    if (e->Inspect(message, controller, inspection_result)) {
      last_service_.store(e, std::memory_order_relaxed);
      return e;
    }
  }
  return nullptr;
}

template <class F>
void NormalConnectionHandler::PrepareForRpc(
    const StreamService::InspectionResult& inspection_result,
    const Controller& controller, F&& cb) {
  // Start a new execution context (as none is present now).
  auto exec_ctx = fiber::ExecutionContext::Create();

  // Prepare the execution context and call `cb`.
  exec_ctx->Execute([&] {
    InitializeSessionContext();

    // Now that we've finished setting up the execution context, let's call
    // user's code (or to be precise, `StreamService`.).
    std::forward<F>(cb)();
  });
}

void NormalConnectionHandler::WaitForRpcCompletion() {
  // If the user does some "fire-and-forget" stuff, by the time `Service::Xxx`k
  // returns, there might still be outstanding operations referencing our
  // session context. Wait for them then.
  //
  // FIXME: We should use some event-driven mechanism to accomplish this.
  //
  // Note that, however, given that most of the time by the time we reach here,
  // all outstanding reference to `exec_ctx` has already vanished, polling below
  // is likely to be satisfied on the first try. This can be more efficient than
  // using a condition variable.
  //
  // If we're going to substitute this with a event-driven mechanism, make sure
  // to account for the above optimization.
  auto exec_ctx = fiber::ExecutionContext::Current();
  while (FLARE_UNLIKELY(exec_ctx->UnsafeRefCount() != 1)) {
    this_fiber::SleepFor(10ms);
  }
}

// Called inside execution context.
void NormalConnectionHandler::InitializeForTracing(
    const StreamService::InspectionResult& inspection_result,
    const Controller& controller) {
  auto&& tracing_ctx = rpc::session_context->tracing;

  // Let's see if we can start a new span here.
  if (auto&& serialized_ctx = controller.GetTracingContext();
      // Either the caller passed a tracing context to us, or we're asked to
      // start a trace unconditionally (on trace missing, as obvious).
      !serialized_ctx.empty() || FLAGS_flare_rpc_start_new_trace_on_missing) {
    auto&& ops = *tracing::GetTracingOps(ctx_->service_name);
    std::unique_ptr<opentracing::SpanContext> incoming_ctx;

    if (!serialized_ctx.empty()) {  // The caller passed a span context to us.
      if (auto opt = ops.ParseSpanContextFrom(serialized_ctx)) {
        incoming_ctx = std::move(*opt);
      } else {
        FLARE_LOG_WARNING(
            "Failed to parse tracing context, starting a new trace.");
        // Fall-through.
      }
    }

    // Initialize the tracing context and start a new span.
    tracing_ctx.tracing_ops = &ops;
    tracing_ctx.server_span =
        // `inspection_result.method` is fully qualified. This complies to
        // what's suggested by OpenTracing standards:
        //
        // > Examples of default operation names:
        // > ...
        // > - The concatenated names of an RPC service and method
        //
        // @sa:
        // https://opentracing.io/docs/best-practices/instrumenting-frameworks
        ops.StartSpanWithLazyOptions(inspection_result.method, [&](auto&& f) {
          f(opentracing::ChildOf(incoming_ctx ? incoming_ctx.get() : nullptr));

          // TJG requires `span_kind` to be set in `start_options`. This is
          // silly, to say the least.
          //
          // Note that setting tag (esp. in TJG's case) is slow.
          f(opentracing::SetTag(opentracing::ext::span_kind,
                                opentracing::ext::span_kind_rpc_server));
        });

    // `incoming_ctx` is going away, I'm not sure if all implementation can
    // accept this situation (as `server_span` may still referencing it.).
  }  // Otherwise left the context untouched, in this case it's treated as if
     // tracing is not enabled.
}

void NormalConnectionHandler::FinishTracing(
    Controller* controller, const StreamService::Context& service_context) {
  auto&& span = rpc::session_context->tracing.server_span;

  if (!span.IsForciblySampled() &&
      service_context.advise_trace_forcibly_sampled) {
    span.AdviseForciblySampled();
  }
  span.SetFrameworkTag(tracing::ext::kInvocationStatus, service_context.status);

  controller->SetTraceForciblySampled(span.IsForciblySampled());
  span.Report();  // Report the span after RPC is completed.
}

// Called inside execution context.
void NormalConnectionHandler::InitializeForDumpingBinlog(
    const StreamService::InspectionResult& inspection_result,
    StreamService::Context* call_context) {
  if (FLARE_UNLIKELY(binlog::AcquireSamplingQuotaForDumping())) {
    // Well this request is sampled.
    //
    // Note that we're in CRITICAL PATH here. Even if we don't expect this path
    // to be taken as much (as the sampling rate shouldn't be too high anyway),
    // any slowdown here has an immediate impact on _this_ request. So be quick.
    rpc::session_context->binlog.correlation_id = binlog::NewCorrelationId();
    rpc::session_context->binlog.dumper.emplace(binlog::GetDumper());

    // Let's save the request first.
    auto&& incoming = rpc::session_context->binlog.dumper->GetIncomingCall();
    incoming->SetStartTimestamp(ReadSteadyClock());
    // Whenever possible, we fill `incoming` in `FinishDumpingBinlog` to avoid
    // delaying request processing.
  }
}

void NormalConnectionHandler::FinishDumpingBinlog(
    const experimental::Uuid& service_uuid,
    const StreamService::InspectionResult& inspection_result,
    const StreamService::Context& service_context) {
  if (auto&& opt = rpc::session_context->binlog.dumper) {
    auto&& incoming = opt->GetIncomingCall();

    incoming->SetCorrelationId(rpc::session_context->binlog.correlation_id);
    incoming->SetServiceName(ctx_->service_name);
    incoming->SetOperationName(std::string(inspection_result.method));
    incoming->SetLocalPeer(ctx_->local_peer);
    incoming->SetRemotePeer(ctx_->remote_peer);
    incoming->SetInvocationStatus(Format("{}", service_context.status));
    incoming->SetHandlerUuid(service_uuid);
    incoming->SetFinishTimestamp(ReadSteadyClock());

    opt->Dump();  // Done asynchronously.
  }
}

bool NormalConnectionHandler::OnNewCall() {
  if (!owner_->OnNewCall()) {
    return false;
  }
  ongoing_requests_.fetch_add(1, std::memory_order_relaxed);
  return true;
}

void NormalConnectionHandler::OnCallCompletion() {
  owner_->OnCallCompletion();
  // We must notify owner first, since we're waiting for our own counter to be
  // zero in `Join()`.

  // Pairs with reading of it in `Join()`.
  FLARE_CHECK_GT(ongoing_requests_.fetch_sub(1, std::memory_order_release), 0);
}

void NormalConnectionHandler::OnStreamClosed(std::uint64_t correlation_id) {
  // Nothing yet.
}

void NormalConnectionHandler::OnStreamCleanup(std::uint64_t correlation_id) {
  std::scoped_lock lk(lock_);
  FLARE_CHECK(streams_.find(correlation_id) != streams_.end());
  auto&& sctx = streams_[correlation_id];

  stream_reaper_->Push(
      [sctx = std::move(sctx)] { sctx.io_adaptor->FlushPendingCalls(); });
  streams_.erase(correlation_id);

  // We only unlock `lock_` after we successfully queue a job into
  // `stream_reaper`. Therefore we always have a reference to the stream. Had we
  // unlocked `lock_` before queueing the job into `stream_reaper`, we'd have a
  // hard time when shutting down the connection.

  // `lock_` unlocked implicitly.
}

}  // namespace flare::rpc::detail
