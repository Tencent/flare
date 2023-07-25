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

#include "flare/rpc/internal/dry_run_connection_handler.h"

#include <chrono>
#include <string>
#include <utility>
#include <vector>

#include "flare/base/deferred.h"
#include "flare/base/logging.h"
#include "flare/base/string.h"
#include "flare/base/tsc.h"
#include "flare/fiber/execution_context.h"
#include "flare/fiber/fiber.h"
#include "flare/fiber/this_fiber.h"
#include "flare/rpc/binlog/log_reader.h"
#include "flare/rpc/internal/session_context.h"
#include "flare/rpc/protocol/stream_service.h"
#include "flare/rpc/server.h"

using namespace std::literals;

namespace flare::rpc::detail {

DryRunConnectionHandler::DryRunConnectionHandler(Server* owner,
                                                 std::unique_ptr<Context> ctx)
    : owner_(owner), ctx_(std::move(ctx)) {}

void DryRunConnectionHandler::Stop() {}

void DryRunConnectionHandler::Join() {
  while (ongoing_requests_.load(std::memory_order_acquire)) {
    this_fiber::SleepFor(100ms);
  }
}

void DryRunConnectionHandler::OnAttach(StreamConnection* conn) { conn_ = conn; }

void DryRunConnectionHandler::OnDetach() {}

void DryRunConnectionHandler::OnWriteBufferEmpty() {}

void DryRunConnectionHandler::OnDataWritten(std::uintptr_t ctx) {}

StreamConnectionHandler::DataConsumptionStatus
DryRunConnectionHandler::OnDataArrival(NoncontiguousBuffer* buffer) {
  ScopedDeferred _([&] { ConsiderUpdateCoarseLastEventTimestamp(); });
  auto dry_runner = binlog::GetDryRunner();
  FLARE_CHECK(dry_runner);

  while (true) {
    std::unique_ptr<binlog::DryRunContext> dry_run_ctx;
    auto status = dry_runner->ParseByteStream(buffer, &dry_run_ctx);
    if (status == binlog::DryRunner::ByteStreamParseStatus::Success) {
      ProcessOneDryRunContext(std::move(dry_run_ctx));
      // Keep looping.
    } else if (status == binlog::DryRunner::ByteStreamParseStatus::NeedMore) {
      return DataConsumptionStatus::Ready;
    } else if (status == binlog::DryRunner::ByteStreamParseStatus::Error) {
      return DataConsumptionStatus::Error;
    }
  }
}

void DryRunConnectionHandler::OnClose() {
  owner_->OnConnectionClosed(ctx_->id);
}

void DryRunConnectionHandler::OnError() { OnClose(); }

bool DryRunConnectionHandler::StartNewCall() {
  if (!owner_->OnNewCall()) {
    return false;
  }
  ongoing_requests_.fetch_add(1, std::memory_order_acq_rel);
  return true;
}

void DryRunConnectionHandler::FinishCall() {
  owner_->OnCallCompletion();
  ongoing_requests_.fetch_sub(1, std::memory_order_release);
}

void DryRunConnectionHandler::ProcessOneDryRunContext(
    std::unique_ptr<binlog::DryRunContext> dry_run_ctx) {
  auto log_reader = std::make_unique<binlog::LogReader>();

  if (!log_reader->InitializeWithProvider(std::move(dry_run_ctx))) {
    FLARE_LOG_WARNING_EVERY_SECOND("Failed to initialize reader for binlog.");
    return;
  }

  auto&& incoming = log_reader->GetIncomingCall();
  auto&& serializer_uuid = incoming->GetHandlerUuid();

  StreamService* handler = nullptr;
  for (auto&& e : ctx_->services) {
    if (e->GetUuid() == serializer_uuid) {
      handler = e;
      break;
    }
  }
  if (!handler) {
    FLARE_LOG_WARNING_EVERY_SECOND("Binlog serializer [{}] is not found.",
                                   serializer_uuid.ToString());
    return;
  }

  if (!StartNewCall()) {
    FLARE_LOG_ERROR_EVERY_SECOND(
        "Failed to start new call. Too many requests pending?");
    return;
  }

  fiber::internal::StartFiberDetached(
      [=, this, log_reader = std::move(log_reader)]() mutable {
        ServiceDryRunFor(std::move(log_reader), handler);
        FinishCall();
      });
}

void DryRunConnectionHandler::ServiceDryRunFor(
    std::unique_ptr<binlog::LogReader> log_reader, StreamService* handler) {
  std::vector<std::string> pkt_ctxs;
  for (auto&& e : log_reader->GetIncomingCall()->GetIncomingPackets()) {
    pkt_ctxs.push_back(e.system_ctx);
  }
  StreamService::ExtractedCall extracted;
  if (!handler->ExtractCall(log_reader->GetIncomingCall()->GetSystemContext(),
                            pkt_ctxs, &extracted)) {
    FLARE_LOG_WARNING_EVERY_SECOND("Failed to deserialize binlog.");
    return;
  }

  FLARE_CHECK(!extracted.messages.empty());

  // FIXME: Do we really have to do this?
  StreamService::InspectionResult inspection_result;
  if (!handler->Inspect(*extracted.messages.front(), *extracted.controller,
                        &inspection_result)) {
    FLARE_LOG_ERROR("Failed to inspect message.");
    return;
  }

  if (extracted.messages.front()->GetType() != Message::Type::Single) {
    FLARE_LOG_ERROR_ONCE("Not implemented: Dry run support for streaming RPC.");
    return;
  }

  StreamService::Context context;

  context.received_tsc = ReadTsc();  // Well the timestamps are fake.
  context.dispatched_tsc = ReadTsc();
  context.parsed_tsc = ReadTsc();
  context.local_peer = ctx_->local_peer;
  context.remote_peer = ctx_->remote_peer;
  context.controller = extracted.controller.get();

  fiber::ExecutionContext::Create()->Execute([&] {
    rpc::InitializeSessionContext();

    rpc::session_context->binlog.correlation_id =
        log_reader->GetIncomingCall()->GetCorrelationId();
    rpc::session_context->binlog.dry_runner = std::move(log_reader);

    auto status = handler->FastCall(
        &extracted.messages.front(),
        [](auto&&) { return 123 /* bytes written */; }, &context);
    if (status != StreamService::ProcessingStatus::Processed) {
      FLARE_LOG_ERROR_EVERY_SECOND("Failed to process request.");
      return;  // What else can we do?
    }
    rpc::session_context->binlog.dry_runner->SetInvocationStatus(
        Format("{}", context.status));

    // Whatever need to be captured is already done by `handler` via
    // `dry_run_ctx->CaptureResponsePacket`, nothing more to do.

    NoncontiguousBuffer response_buffer;
    rpc::session_context->binlog.dry_runner->WriteReport(&response_buffer);
    conn_->Write(std::move(response_buffer), 0);
  });
}

}  // namespace flare::rpc::detail
