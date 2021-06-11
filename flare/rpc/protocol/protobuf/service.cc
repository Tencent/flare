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

#define FLARE_RPC_SERVER_CONTROLLER_SUPPRESS_INCLUDE_WARNING

#include "flare/rpc/protocol/protobuf/service.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "gflags/gflags.h"
#include "protobuf/descriptor.h"

#include "flare/base/callback.h"
#include "flare/base/down_cast.h"
#include "flare/base/string.h"
#include "flare/rpc/internal/fast_latch.h"
#include "flare/rpc/internal/rpc_metrics.h"
#include "flare/rpc/internal/session_context.h"
#include "flare/rpc/protocol/protobuf/binlog.h"
#include "flare/rpc/protocol/protobuf/binlog.pb.h"
#include "flare/rpc/protocol/protobuf/call_context.h"
#include "flare/rpc/protocol/protobuf/rpc_options.h"
#include "flare/rpc/protocol/protobuf/rpc_server_controller.h"
#include "flare/rpc/protocol/protobuf/service_method_locator.h"
#include "flare/rpc/protocol/stream_protocol.h"
#include "flare/rpc/rpc_options.pb.h"
#include "flare/rpc/tracing/framework_tags.h"

using namespace std::literals;

DEFINE_string(flare_rpc_server_protocol_buffers_max_ongoing_requests_per_method,
              "",
              "If set, a list of method_full_name:limit, separated by comma, "
              "should be provided. This flag controls allowed maximum "
              "concurrent requests, in a per-method fashion. e.g.: "
              "`flare.example.EchoService.Echo:10000,flare.example.EchoService."
              "Echo2:5000`. If both this option and Protocol Buffers option "
              "`flare.max_ongoing_requests` are applicable, the smaller one "
              "is respected.");

namespace flare::protobuf {

namespace {

ProtoMessage CreateErrorResponse(std::uint64_t correlation_id,
                                 rpc::Status status, std::string description) {
  FLARE_CHECK(status != rpc::STATUS_SUCCESS);
  auto meta = object_pool::Get<rpc::RpcMeta>();
  meta->set_correlation_id(correlation_id);
  meta->set_method_type(rpc::METHOD_TYPE_SINGLE);
  meta->mutable_response_meta()->set_status(status);
  meta->mutable_response_meta()->set_description(std::move(description));
  return ProtoMessage(std::move(meta), nullptr);
}

std::unordered_map<std::string, std::uint32_t> ParseMaxOngoingRequestFlag() {
  std::unordered_map<std::string, std::uint32_t> result;

  auto splitted = Split(
      FLAGS_flare_rpc_server_protocol_buffers_max_ongoing_requests_per_method,
      ",");
  for (auto&& e : splitted) {
    auto method_limit = Split(e, ":");
    FLARE_CHECK(method_limit.size() == 2,
                "Invalid per-method max-ongoing-requests config: [{}]", e);
    auto name = std::string(method_limit[0]);
    auto method_desc =
        google::protobuf::DescriptorPool::generated_pool()->FindMethodByName(
            name);
    FLARE_CHECK(method_desc, "Unrecognized method [{}].", method_limit[0]);
    auto limit = TryParse<std::uint32_t>(method_limit[1]);
    FLARE_CHECK(limit, "Invalid max-ongoing-request limit [{}].",
                method_limit[1]);
    result[name] = *limit;
  }
  return result;
}

}  // namespace

Service::~Service() {
  for (auto&& e : service_descs_) {
    ServiceMethodLocator::Instance()->DeleteService(e);
  }
}

void Service::AddService(MaybeOwning<google::protobuf::Service> impl) {
  static const auto kMaxOngoingRequestsConfigs = ParseMaxOngoingRequestFlag();

  auto&& service_desc = impl->GetDescriptor();

  for (int i = 0; i != service_desc->method_count(); ++i) {
    auto method = service_desc->method(i);
    auto name = method->full_name();

    FLARE_CHECK(method_descs_.find(name) == method_descs_.end(),
                "Duplicate method: {}", name);
    auto&& e = method_descs_[name];

    // Basics.
    e.service = impl.Get();
    e.method = method;
    e.request_prototype =
        google::protobuf::MessageFactory::generated_factory()->GetPrototype(
            method->input_type());
    e.response_prototype =
        google::protobuf::MessageFactory::generated_factory()->GetPrototype(
            method->output_type());
    e.is_streaming = IsStreamingMethod(method);

    // Limit on maximum delay in dispatch queue.
    if (auto delay =
            method->options().GetExtension(flare::max_queueing_delay_ms)) {
      e.max_queueing_delay = 1ms * delay;
    }

    // Limit on maximum concurency.
    e.max_ongoing_requests = std::numeric_limits<std::uint32_t>::max();
    if (method->options().HasExtension(flare::max_ongoing_requests)) {
      e.max_ongoing_requests =
          method->options().GetExtension(flare::max_ongoing_requests);
    }
    if (auto iter = kMaxOngoingRequestsConfigs.find(name);
        iter != kMaxOngoingRequestsConfigs.end()) {
      e.max_ongoing_requests = std::min(e.max_ongoing_requests, iter->second);
    }
    if (e.max_ongoing_requests != std::numeric_limits<std::uint32_t>::max()) {
      e.ongoing_requests = std::make_unique<AlignedInt>();
    }

    rpc::detail::RpcMetrics::Instance()->RegisterMethod(method);
  }

  services_.push_back(std::move(impl));
  ServiceMethodLocator::Instance()->AddService(
      services_.back()->GetDescriptor());
  service_descs_.push_back(services_.back()->GetDescriptor());
  registered_services_.insert(services_.back()->GetDescriptor()->full_name());
}

const experimental::Uuid& Service::GetUuid() const noexcept {
  static constexpr experimental::Uuid kUuid(
      "7D3B4ED4-D35E-46E0-87BD-2A03915D1760");
  return kUuid;
}

bool Service::Inspect(const Message& message, const Controller& controller,
                      InspectionResult* result) {
  if (auto msg = dyn_cast<ProtoMessage>(message); FLARE_LIKELY(msg)) {
    result->method = msg->meta->request_meta().method_name();
    return true;
  } else if (isa<EarlyErrorMessage>(message)) {
    result->method = "(unrecognized method)";
    return true;  // `result->method` is not filled. We don't recognize the
                  // method being called anyway.
  }
  return false;
}

bool Service::ExtractCall(const std::string& serialized_ctx,
                          const std::vector<std::string>& serialized_pkt_ctxs,
                          ExtractedCall* extracted) {
  if (serialized_pkt_ctxs.size() != 1) {
    FLARE_LOG_ERROR_ONCE("Not supported: Performing streaming RPC dry-run.");
    return false;
  }
  rpc::SerializedServerPacket call;
  if (!call.ParseFromString(serialized_pkt_ctxs[0])) {
    FLARE_LOG_WARNING_EVERY_SECOND("Failed to deserialize RPC binlog.");
    return false;
  }
  if (call.streaming_rpc()) {
    // TODO(luobogao): Support streaming RPC.
    FLARE_LOG_ERROR_ONCE(
        "Not implemented: Deserialize RPC binlog for streaming RPC.");
    return false;
  }

  auto desc = FindHandler(call.method());
  if (!desc) {
    FLARE_LOG_WARNING_EVERY_SECOND("Unknown method [{}] is requested.",
                                   call.method());
    return false;
  }

  std::unique_ptr<google::protobuf::Message> msg_body{
      desc->request_prototype->New()};
  if (!msg_body->ParseFromString(call.body())) {
    FLARE_LOG_WARNING_EVERY_SECOND(
        "Failed to parse request body as [{}].",
        desc->request_prototype->GetDescriptor()->full_name());
    return false;
  }

  auto msg = std::make_unique<ProtoMessage>();
  msg->meta = object_pool::Get<rpc::RpcMeta>();
  msg->meta->set_correlation_id(0);  // Does not matter, *I think*.
  msg->meta->set_method_type(rpc::METHOD_TYPE_SINGLE);
  msg->meta->mutable_request_meta()->set_method_name(call.method());
  msg->msg_or_buffer = std::move(msg_body);
  msg->attachment = CreateBufferSlow(call.attachment());

  // Let's fill the result.
  extracted->messages.push_back(std::move(msg));
  extracted->controller = std::make_unique<PassiveCallContext>();
  return true;
}

Service::ProcessingStatus Service::FastCall(
    std::unique_ptr<Message>* request,
    const FunctionView<std::size_t(const Message&)>& writer, Context* context) {
  // Do some sanity check first.
  auto method_desc =
      SanityCheckOrRejectEarlyForFastCall(**request, writer, *context);
  if (FLARE_UNLIKELY(!method_desc)) {
    return ProcessingStatus::Processed;
  }

  auto req_msg = cast<ProtoMessage>(**request);
  auto processing_quota =
      AcquireProcessingQuotaOrReject(*req_msg, *method_desc, *context);
  if (!processing_quota) {
    return ProcessingStatus::Overloaded;
  }

  // Initialize server RPC controller.
  RpcServerController rpc_controller;
  InitializeServerControllerForFastCall(*req_msg, *context, &rpc_controller);

  // Call user's implementation and sent response out.
  ProtoMessage resp_msg;
  InvokeUserMethodForFastCall(*method_desc, *req_msg, &resp_msg,
                              &rpc_controller, writer, context);

  // Finish tracing / binlog stuff.
  CompleteTracingPostOperationForFastCall(&rpc_controller, context);
  CompleteBinlogPostOperationForFastCall(*req_msg, resp_msg, rpc_controller,
                                         context);

  return ProcessingStatus::Processed;
}

StreamService::ProcessingStatus Service::StreamCall(
    AsyncStreamReader<std::unique_ptr<Message>>* input_stream,
    AsyncStreamWriter<std::unique_ptr<Message>>* output_stream,
    Context* context) {
  if (FLARE_UNLIKELY(rpc::session_context->binlog.dumper)) {
    FLARE_LOG_ERROR_ONCE(
        "RPC binlog is not supported by streaming RPC (yet.).");
    rpc::session_context->binlog.dumper->Abort();
  }

  auto msg_ptr = cast<ProtoMessage>(
      fiber::BlockingGet(input_stream->Peek())->value().get());
  if (!msg_ptr->attachment.Empty()) {
    FLARE_LOG_WARNING_EVERY_SECOND(
        "Unsupported: Attachment is not allowed in streaming RPC.");
    return ProcessingStatus::Corrupted;
  }
  FLARE_CHECK(msg_ptr->meta->has_request_meta());

  auto&& method_name = msg_ptr->meta->request_meta().method_name();
  auto&& method_desc = FindHandler(method_name);
  FLARE_CHECK(method_desc);

  if (FLARE_UNLIKELY(!method_desc->is_streaming)) {
    return ProcessingStatus::Corrupted;
  }

  auto processing_quota =
      AcquireProcessingQuotaOrReject(*msg_ptr, *method_desc, *context);
  if (!processing_quota) {
    return ProcessingStatus::Overloaded;
  }

  RpcServerController rpc_controller;
  rpc_controller.SetIsStreaming();

  rpc_controller.SetRemotePeer(context->remote_peer);
  // rpc_controller.SetLocalPeer(context->local_peer);
  rpc_controller.SetAcceptableCompressionAlgorithm(
      msg_ptr->meta->request_meta().acceptable_compression_algorithms());
  if (auto v = msg_ptr->meta->request_meta().timeout()) {
    // `received_tsc` is the most accurate timestamp we can get. However, there
    // can still be plenty of time elapsed on the network.
    rpc_controller.SetTimeout(TimestampFromTsc(context->received_tsc) +
                              v * 1ms);
  }
  if (auto&& ctx = rpc::session_context->binlog;
      FLARE_UNLIKELY(ctx.dry_runner)) {
    rpc_controller.SetInDryRunEnvironment();
    rpc_controller.SetBinlogCorrelationId(ctx.correlation_id);
    // Copy user tags if we're in dry-run env.
    for (auto&& [k, v] : ctx.dry_runner->GetIncomingCall()->GetUserTags()) {
      (*rpc_controller.MutableUserBinlogTagsForRead())[k] = v;
    }
  } else if (FLARE_UNLIKELY(ctx.dumper)) {
    rpc_controller.SetIsCapturingBinlog(true);
    rpc_controller.SetBinlogCorrelationId(ctx.correlation_id);
  }

  rpc::RpcMeta response_meta_prototype;
  response_meta_prototype.set_correlation_id(msg_ptr->GetCorrelationId());
  response_meta_prototype.set_method_type(rpc::METHOD_TYPE_STREAM);
  // Changed by `RpcServerController::SetFailed()` when necessary.
  response_meta_prototype.mutable_response_meta()->set_status(
      rpc::STATUS_SUCCESS);

  // Well in certain cases we still need a valid request / response pointer.
  const google::protobuf::Message* request = nullptr;
  std::unique_ptr<google::protobuf::Message> response = nullptr;
  if (IsClientStreamingMethod(method_desc->method)) {
    rpc_controller.SetStreamReader(std::move(*input_stream));
  } else {
    // In case the request is a single message, it should be passed to user's
    // code via `request` parameter. In this case passing it via `StreamReader`
    // is counter-intuitive.
    FLARE_CHECK(
        msg_ptr->msg_or_buffer.index() == 1,
        "Receiving request in bytes is not supported in streaming RPC.");
    request = std::get<1>(msg_ptr->msg_or_buffer).Get();
  }
  if (IsServerStreamingMethod(method_desc->method)) {
    rpc_controller.SetStreamWriter(std::move(*output_stream));
    rpc_controller.SetRpcMetaPrototype(std::move(response_meta_prototype));
    // Timestamp is NOT set, it's hard to define a *single* timestamp when there
    // are multiple messages are involved.
    if (context->streaming_call_no_eos_marker) {
      rpc_controller.DisableEndOfStreamMarker();
    }
  } else {
    // In case the server side is not returning a stream of responses, we should
    // use use `response` passed to user's code to hold the response. It
    // wouldn't be counter-intuitive for non-server-side-streaming code to
    // return result via `rpc_ctlr->GetStreamWriter()`.
    response.reset(method_desc->response_prototype->New());
  }

  rpc::detail::FastLatch fast_latch;
  internal::LocalCallback done_callback([&] { fast_latch.count_down(); });
  method_desc->service->CallMethod(method_desc->method, &rpc_controller,
                                   request, response.get(), &done_callback);
  fast_latch.wait();

  if (!IsClientStreamingMethod(method_desc->method)) {
    fiber::BlockingGet(input_stream->Close());
  }
  if (!IsServerStreamingMethod(method_desc->method)) {
    auto resp_msg = std::make_unique<ProtoMessage>();

    resp_msg->meta = object_pool::Get<rpc::RpcMeta>();
    *resp_msg->meta = response_meta_prototype;
    resp_msg->meta->set_flags(rpc::MESSAGE_FLAGS_START_OF_STREAM |
                              rpc::MESSAGE_FLAGS_END_OF_STREAM);
    if (rpc_controller.Failed()) {
      resp_msg->meta->set_flags(resp_msg->meta->flags() |
                                rpc::MESSAGE_FLAGS_NO_PAYLOAD);
      resp_msg->meta->mutable_response_meta()->set_status(
          rpc_controller.ErrorCode());
      resp_msg->meta->mutable_response_meta()->set_description(
          rpc_controller.ErrorText());
    } else {
      resp_msg->msg_or_buffer = std::move(response);
    }
    FLARE_LOG_WARNING_IF_EVERY_SECOND(
        !fiber::BlockingGet(output_stream->WriteLast(std::move(resp_msg))),
        "Failed to write response.");
  }

  if (auto&& opt = rpc::session_context->binlog.dumper; FLARE_UNLIKELY(opt)) {
    for (auto&& [k, v] : rpc_controller.GetUserBinlogTagsForWrite()) {
      opt->GetIncomingCall()->SetUserTag(k, v);
    }
  }

  rpc::detail::RpcMetrics::Instance()->Report(
      method_desc->method, rpc_controller.ErrorCode(),
      rpc_controller.GetElapsedTime() / 1ms,
      // TODO(luobogao): Record bytes we've read / written during this RPC.
      0, 0);
  FLARE_CHECK(rpc_controller.GetResponseAttachment().Empty(),
              "Attachment is not supported in streaming RPC.");
  FLARE_CHECK(!rpc_controller.HasResponseRawBytes(),
              "Sending response from bytes is not supported in streaming RPC.");

  // TODO(luobogao): Tracing is not implemented.
  return ProcessingStatus::Processed;
}

void Service::Stop() {
  // Nothing.
  //
  // Outstanding requests are counted by `Server`, we don't have to bother doing
  // that.)
}

void Service::Join() {
  // Nothing.
}

// Well it's slow. Yet it's only called *after* we've sent the response. RPC
// latency is not affected (unless we're bounded by CPU in the meantime),
// nevermind.
void Service::WriteFastCallBinlog(const ProtoMessage& req,
                                  const ProtoMessage& resp) {
  if (rpc::session_context->binlog.dumper->Dumping()) {  // Not aborted then.
    auto&& incoming = rpc::session_context->binlog.dumper->GetIncomingCall();

    // We need this one to reconstruct the request in dry-run mode.
    rpc::SerializedServerPacket serialized;
    serialized.set_streaming_rpc(false);
    serialized.set_using_raw_bytes(req.msg_or_buffer.index() == 2);
    serialized.set_method(req.meta->request_meta().method_name());
    serialized.set_body(flare::FlattenSlow(Write(req.msg_or_buffer)));
    serialized.set_attachment(FlattenSlow(req.attachment));

    // Now notify the framework.
    incoming->AddIncomingPacket(WritePacketDesc(req),
                                serialized.SerializeAsString());
    incoming->AddOutgoingPacket(WritePacketDesc(resp));

  } else {
    rpc::session_context->binlog.dumper->Abort();
  }
}

void Service::CaptureFastCallDryRunResult(const ProtoMessage& req,
                                          const ProtoMessage& resp) {
  rpc::session_context->binlog.dry_runner->GetIncomingCall()
      ->CaptureOutgoingPacket(WritePacketDesc(resp));

  // We don't care about `req` here.
}

const Service::MethodDesc* Service::SanityCheckOrRejectEarlyForFastCall(
    const Message& msg,
    const FunctionView<std::size_t(const Message&)>& resp_writer,
    const Context& ctx) const {
  auto msg_ptr = dyn_cast<ProtoMessage>(msg);
  if (FLARE_UNLIKELY(!msg_ptr)) {
    auto e = dyn_cast<EarlyErrorMessage>(msg);
    FLARE_CHECK(e);  // Otherwise either the framework or `Inspect` is
    // misbehaving.
    resp_writer(CreateErrorResponse(e->GetCorrelationId(), e->GetStatus(),
                                    e->GetDescription()));
    return nullptr;
  }

  // Otherwise our protocol lack some basic sanity checks.
  FLARE_CHECK(msg_ptr->meta->has_request_meta());

  // Note that even if our protocol object recognizes the method, it's possible
  // that the service the method belongs to, is not registered with us. If the
  // server is serving different "service" on different port, this can be the
  // case.
  auto&& method_name = msg_ptr->meta->request_meta().method_name();
  auto&& method_desc = FindHandler(method_name);
  if (FLARE_UNLIKELY(!method_desc)) {
    std::string_view service_name = method_name;
    if (auto pos = service_name.find_last_of('.');
        pos != std::string_view::npos) {
      service_name = service_name.substr(0, pos);
    }
    if (registered_services_.count(service_name) == 0) {
      resp_writer(CreateErrorResponse(
          msg_ptr->GetCorrelationId(), rpc::STATUS_SERVICE_NOT_FOUND,
          Format("Service [{}] is not found.", service_name)));
    } else {
      resp_writer(CreateErrorResponse(
          msg_ptr->GetCorrelationId(), rpc::STATUS_METHOD_NOT_FOUND,
          Format("Method [{}] is not found.", method_name)));
    }
    return nullptr;
  }

  if (FLARE_UNLIKELY(method_desc->is_streaming)) {
    resp_writer(CreateErrorResponse(
        msg_ptr->GetCorrelationId(), rpc::STATUS_MALFORMED_DATA,
        Format("You're calling a streaming method in non-streaming way.")));
    return nullptr;
  }
  return method_desc;
}

void Service::InitializeServerControllerForFastCall(const ProtoMessage& msg,
                                                    const Context& ctx,
                                                    RpcServerController* ctlr) {
  ctlr->SetRemotePeer(ctx.remote_peer);

  // Start timestamp is set as the same as the packet was received -- We don't
  // want the time to go backward. Were start timestamp specified as now, the
  // packet should have been received even before this RPC starts.
  ctlr->SetTimestamp(RpcServerController::Timestamp::Start, ctx.received_tsc);
  ctlr->SetTimestamp(RpcServerController::Timestamp::Received,
                     ctx.received_tsc);
  ctlr->SetTimestamp(RpcServerController::Timestamp::Dispatched,
                     ctx.dispatched_tsc);
  ctlr->SetTimestamp(RpcServerController::Timestamp::Parsed, ctx.parsed_tsc);
  ctlr->SetAcceptableCompressionAlgorithm(
      msg.meta->request_meta().acceptable_compression_algorithms());
  if (auto v = msg.meta->request_meta().timeout()) {
    // `received_tsc` is the most accurate timestamp we can get. However, there
    // can still be plenty of time elapsed on the network.
    ctlr->SetTimeout(TimestampFromTsc(ctx.received_tsc) + v * 1ms);
  }
  if (FLARE_UNLIKELY(!msg.attachment.Empty())) {
    ctlr->SetRequestAttachment(msg.attachment);
  }

  // Set binlog flags if necessary.
  if (auto&& ctx = rpc::session_context->binlog;
      FLARE_UNLIKELY(ctx.dry_runner)) {
    ctlr->SetInDryRunEnvironment();
    ctlr->SetBinlogCorrelationId(ctx.correlation_id);
    // Copy user tags if we're in dry-run env.
    for (auto&& [k, v] : ctx.dry_runner->GetIncomingCall()->GetUserTags()) {
      (*ctlr->MutableUserBinlogTagsForRead())[k] = v;
    }
  } else if (FLARE_UNLIKELY(ctx.dumper)) {
    ctlr->SetIsCapturingBinlog(true);
    ctlr->SetBinlogCorrelationId(ctx.correlation_id);
  }
}

void Service::InvokeUserMethodForFastCall(
    const MethodDesc& method, const ProtoMessage& req_msg,
    ProtoMessage* resp_msg, RpcServerController* ctlr,
    const FunctionView<std::size_t(const Message&)>& writer, Context* ctx) {
  // Prepare response message.
  auto resp_ptr = std::unique_ptr<google::protobuf::Message>(
      method.response_prototype->New());

  // For better responsiveness, we allow the user to write response early via
  // `RpcServerController::WriteResponseImmediately` (or, if not called, once
  // `done` is called, so we have to provide a callback to fill and write the
  // response.
  internal::LocalCallback write_resp_callback([&] {
    CreateNativeResponse(method, req_msg, std::move(resp_ptr), ctlr, resp_msg);

    // Note that `writer` does not mutate `resp_msg`, we rely on this as we'll
    // still need the response later.
    auto bytes = writer(*resp_msg);
    rpc::detail::RpcMetrics::Instance()->Report(
        method.method, ctlr->ErrorCode(), ctlr->GetElapsedTime() / 1ms,
        ctx->incoming_packet_size, bytes);
  });
  ctlr->SetEarlyWriteResponseCallback(&write_resp_callback);

  // We always call the callback in a synchronous fashion. Given that our fiber
  // runtime is fairly fast, there's no point in implementing method invocation
  // in a "asynchronous" fashion, which is even slower.
  rpc::detail::FastLatch done_latch;
  internal::LocalCallback done_callback([&] {
    // If the user did not call `WriteResponseImmediately` (likely), let's call
    // it for them.
    if (auto ptr = ctlr->DestructiveGetEarlyWriteResponse()) {
      ptr->Run();
    }
    done_latch.count_down();
  });

  method.service->CallMethod(method.method, ctlr,
                             FLARE_LIKELY(req_msg.msg_or_buffer.index() == 1)
                                 ? std::get<1>(req_msg.msg_or_buffer).Get()
                                 : nullptr,
                             resp_ptr.get(), &done_callback);
  done_latch.wait();

  // Save the result for later use.
  ctx->status = ctlr->ErrorCode();
}

void Service::CompleteTracingPostOperationForFastCall(RpcServerController* ctlr,
                                                      Context* ctx) {
  auto&& span = rpc::session_context->tracing.server_span;
  if (FLARE_UNLIKELY(ctlr->Failed())) {
    ctx->advise_trace_forcibly_sampled = true;
  }
  // We only set the tags if the span is indeed going to be sampled.
  if (FLARE_UNLIKELY(span.Tracing() || ctx->advise_trace_forcibly_sampled)) {
    for (auto&& [k, v] : *ctlr->MutableUserTracingTags()) {
      span.SetUserTag(std::move(k), std::move(v));
    }
    for (auto&& [k, v] : *ctlr->MutableUserTracingLogs()) {
      span.Log(std::move(k), std::move(v));
    }
  }
}

void Service::CompleteBinlogPostOperationForFastCall(
    const ProtoMessage& req, const ProtoMessage& resp,
    const RpcServerController& ctlr, Context* ctx) {
  if (auto&& opt = rpc::session_context->binlog.dumper; FLARE_UNLIKELY(opt)) {
    for (auto&& [k, v] : ctlr.GetUserBinlogTagsForWrite()) {
      opt->GetIncomingCall()->SetUserTag(k, v);
    }
    if (ctlr.IsCapturingBinlog()) {
      WriteFastCallBinlog(req, resp);
    } else {
      opt->Abort();
    }
  }
  if (FLARE_UNLIKELY(rpc::session_context->binlog.dry_runner)) {
    CaptureFastCallDryRunResult(req, resp);
  }
}

Deferred Service::AcquireProcessingQuotaOrReject(const ProtoMessage& msg,
                                                 const MethodDesc& method,
                                                 const Context& ctx) {
  auto max_queueing_delay = method.max_queueing_delay;
  if (auto v = msg.meta->request_meta().timeout()) {
    max_queueing_delay =
        std::min<std::chrono::nanoseconds>(max_queueing_delay, v * 1ms);
  }
  if (FLARE_UNLIKELY(DurationFromTsc(ctx.received_tsc, ReadTsc()) >
                     max_queueing_delay)) {
    FLARE_LOG_WARNING_EVERY_SECOND(
        "Rejecting call to [{}] from [{}]: It has been in queue for too long.",
        msg.meta->request_meta().method_name(), ctx.remote_peer.ToString());
    return Deferred();
  }

  auto&& ongoing_req_ptr = method.ongoing_requests.get();
  if (FLARE_UNLIKELY(
          ongoing_req_ptr &&
          ongoing_req_ptr->value.fetch_add(1, std::memory_order_relaxed) + 1 >
              method.max_ongoing_requests)) {
    ongoing_req_ptr->value.fetch_sub(1, std::memory_order_relaxed);
    FLARE_LOG_WARNING_EVERY_SECOND(
        "Rejecting call to [{}] from [{}]: Too many concurrent requests.",
        msg.meta->request_meta().method_name(), ctx.remote_peer.ToString());
    return Deferred();
  }

  return Deferred([ongoing_req_ptr] {
    // Restore ongoing request counter.
    if (ongoing_req_ptr) {
      FLARE_CHECK_GE(
          ongoing_req_ptr->value.fetch_sub(1, std::memory_order_relaxed), 0);
    }
  });
}

void Service::CreateNativeResponse(
    const MethodDesc& method_desc, const ProtoMessage& request,
    std::unique_ptr<google::protobuf::Message> resp_ptr,
    RpcServerController* ctlr, ProtoMessage* response) {
  // Message meta goes first.
  auto meta = object_pool::Get<rpc::RpcMeta>();
  meta->set_correlation_id(request.GetCorrelationId());
  meta->set_method_type(rpc::METHOD_TYPE_SINGLE);
  if (auto algo = ctlr->GetCompressionAlgorithm();
      algo != rpc::COMPRESSION_ALGORITHM_NONE) {
    FLARE_LOG_WARNING_IF(!ctlr->GetAcceptableCompressionAlgorithms()[algo],
                         "Using unacceptable compression algorithm [{}]. The "
                         "client is likely to fail to decode response.",
                         algo);
    meta->set_compression_algorithm(algo);
    meta->set_attachment_compressed(true);
  }

  auto&& resp_meta = *meta->mutable_response_meta();
  resp_meta.set_status(ctlr->ErrorCode());
  if (FLARE_UNLIKELY(ctlr->Failed())) {
    resp_meta.set_description(ctlr->ErrorText());
  }

  response->meta = std::move(meta);

  // Let's fill the message body then.
  if (FLARE_UNLIKELY(ctlr->HasResponseRawBytes())) {
    response->msg_or_buffer = ctlr->GetResponseRawBytes();

#ifndef NDEBUG
    // Check if the response really can be deserialized as the response message.
    //
    // Being slow here does not matter as it's only checked in debug builds.

    std::unique_ptr<google::protobuf::Message> checker{
        google::protobuf::MessageFactory::generated_factory()
            ->GetPrototype(method_desc.method->input_type())
            ->New()};
    FLARE_DCHECK(
        checker->ParseFromString(FlattenSlow(ctlr->GetResponseRawBytes())),
        "You're writing a byte stream that is not a valid binary "
        "representation of message [{}].",
        checker->GetDescriptor()->full_name());
#endif
  } else {
    response->msg_or_buffer = std::move(resp_ptr);
  }

  // And the attachment.
  if (auto&& att = ctlr->GetResponseAttachment(); !att.Empty()) {
    response->attachment = att;
    response->precompressed_attachment =
        ctlr->GetResponseAttachmentPrecompressed();
  }
}

inline const Service::MethodDesc* Service::FindHandler(
    const std::string& method_name) const {
  return method_descs_.TryGet(method_name);
}

}  // namespace flare::protobuf
