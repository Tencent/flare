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

#define FLARE_RPC_CHANNEL_SUPPRESS_INCLUDE_WARNING
#define FLARE_RPC_CLIENT_CONTROLLER_SUPPRESS_INCLUDE_WARNING

#include "flare/rpc/protocol/protobuf/rpc_channel.h"

#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "gflags/gflags.h"
#include "opentracing/ext/tags.h"

#include "flare/base/buffer/zero_copy_stream.h"
#include "flare/base/callback.h"
#include "flare/base/endian.h"
#include "flare/base/function.h"
#include "flare/base/internal/annotation.h"
#include "flare/base/net/endpoint.h"
#include "flare/base/object_pool.h"
#include "flare/base/random.h"
#include "flare/base/string.h"
#include "flare/base/tsc.h"
#include "flare/fiber/latch.h"
#include "flare/rpc/binlog/dry_runner.h"
#include "flare/rpc/internal/correlation_id.h"
#include "flare/rpc/internal/error_stream_provider.h"
#include "flare/rpc/internal/session_context.h"
#include "flare/rpc/internal/stream_call_gate.h"
#include "flare/rpc/message_dispatcher_factory.h"
#include "flare/rpc/protocol/protobuf/binlog.pb.h"
#include "flare/rpc/protocol/protobuf/message.h"
#include "flare/rpc/protocol/protobuf/mock_channel.h"
#include "flare/rpc/protocol/protobuf/rpc_channel_for_dry_run.h"
#include "flare/rpc/protocol/protobuf/rpc_client_controller.h"
#include "flare/rpc/protocol/protobuf/rpc_meta.pb.h"
#include "flare/rpc/protocol/protobuf/rpc_options.h"
#include "flare/rpc/protocol/protobuf/service_method_locator.h"
#include "flare/rpc/tracing/framework_tags.h"
#include "flare/rpc/tracing/tracing_ops.h"

DEFINE_int32(flare_rpc_channel_max_packet_size, 4 * 1024 * 1024,
             "Default maximum packet size of `RpcChannel`.");

using namespace std::literals;
using flare::rpc::internal::StreamCallGate;

namespace flare {

template <class T>
using Factory = Function<std::unique_ptr<T>()>;

namespace {

struct FastCallContext {
  std::uintptr_t nslb_ctx{};
  PooledPtr<protobuf::ProactiveCallContext> call_ctx;
  rpc::internal::StreamCallGateHandle call_gate_handle;
  tracing::QuickerSpan tracing_span;
  bool multiplexable;
};

protobuf::detail::MockChannel* mock_channel;

bool IsMockAddress(const std::string& address) {
  return StartsWith(address, "mock://");
}

std::pair<AsyncStreamReader<std::unique_ptr<Message>>,
          AsyncStreamWriter<std::unique_ptr<Message>>>
GetErrorStreams() {
  using E = std::unique_ptr<Message>;
  return std::pair(
      AsyncStreamReader<E>(
          MakeRefCounted<rpc::detail::ErrorStreamReaderProvider<E>>()),
      AsyncStreamWriter<E>(
          MakeRefCounted<rpc::detail::ErrorStreamWriterProvider<E>>()));
}

void EnsureBytesOfInputTypeInDebugMode(
    const google::protobuf::MethodDescriptor* method,
    const NoncontiguousBuffer& buffer) {
#ifndef NDEBUG
  // Being slow does not matter as it's only compiled in debug builds.
  std::unique_ptr<google::protobuf::Message> checker{
      google::protobuf::MessageFactory::generated_factory()
          ->GetPrototype(method->input_type())
          ->New()};
  FLARE_DCHECK(checker->ParseFromString(FlattenSlow(buffer)),
               "Byte stream you're providing is not a valid binary "
               "representation of message [{}].",
               checker->GetDescriptor()->full_name());
#endif
}

// Handle the difference between URI scheme difference between our old framework
// and Flare.
//
// This method is only used by `RpcChannel`, so we can assume Protocol Buffers
// protocol here.
void NormalizeUriScheme(std::string* uri) {
  static const std::unordered_map<std::string_view, std::string_view>
      kSchemeMap = {{"http", "http+pb"}, {"qzone", "qzone-pb"}};

  auto pos = uri->find_first_of(':');
  if (pos == std::string::npos) {
    return;  // There's likely an error in URI, let's the caller handle it.
  }
  auto scheme = std::string_view(*uri).substr(0, pos);
  if (auto iter = kSchemeMap.find(scheme); iter == kSchemeMap.end()) {
    return;  // The scheme is in normalized form then.
  } else {
    *uri = std::string(iter->second) + uri->substr(pos);
    // Should we log a warning here?
  }
}

// Returns: scheme, address.
std::optional<std::pair<std::string, std::string>> InspectUri(
    const std::string& uri) {
  static constexpr auto kSep = "://"sv;

  auto colon = uri.find(':');
  if (colon == std::string::npos) {
    return std::nullopt;
  }
  if (uri.size() < colon + kSep.size() ||
      uri.substr(colon, kSep.size()) != kSep) {
    return std::nullopt;
  }
  auto scheme = uri.substr(0, colon);
  auto address = uri.substr(colon + kSep.size());
  return std::pair(scheme, address);
}

std::unique_ptr<MessageDispatcher> NewMessageDispatcherFromName(
    std::string_view name) {
  if (auto pos = name.find_first_of('+'); pos != std::string_view::npos) {
    return MakeCompositedMessageDispatcher(name.substr(0, pos),
                                           name.substr(pos + 1));
  } else {
    return message_dispatcher_registry.TryNew(name);
  }
}

// `Random()` does not perform quite well, besides, we don't need a "real"
// random number for NSLB either, so we use a thread-local RR ID for default
// NSLB key.
std::uint64_t GetNextPseudoRandomKey() {
  FLARE_INTERNAL_TLS_MODEL thread_local std::uint64_t index = Random();
  return index++;
}

rpc::Status TranslateRpcError(StreamCallGate::CompletionStatus status) {
  FLARE_CHECK(status != StreamCallGate::CompletionStatus::Success);
  if (status == StreamCallGate::CompletionStatus::IoError) {
    return rpc::STATUS_IO_ERROR;
  } else if (status == StreamCallGate::CompletionStatus::Timeout) {
    return rpc::STATUS_TIMEOUT;
  } else if (status == StreamCallGate::CompletionStatus::ParseError) {
    return rpc::STATUS_MALFORMED_DATA;
  }
  FLARE_UNREACHABLE();
}

LoadBalancer::Status GetLoadBalancerFeedbackStatusFrom(int rpc_status) {
  if (FLARE_LIKELY(rpc_status == rpc::STATUS_SUCCESS)) {
    return LoadBalancer::Status::Success;
  }
  if (rpc_status == rpc::STATUS_FAILED || rpc_status == rpc::STATUS_FROM_USER ||
      rpc_status > rpc::STATUS_RESERVED_MAX) {
    return LoadBalancer::Status::Success;
  }
  if (rpc_status == rpc::STATUS_OVERLOADED) {
    return LoadBalancer::Status::Overloaded;
  } else if (rpc_status != rpc::STATUS_SUCCESS) {
    return LoadBalancer::Status::Failed;
  }
  FLARE_UNREACHABLE();
}

std::string WriteBinlogContext(const RpcClientController& ctlr,
                               const google::protobuf::Message* response) {
  // TODO(luobogao): Support streaming RPC.
  rpc::SerializedClientPacket serialized;
  serialized.set_streaming_rpc(false);
  serialized.set_using_raw_bytes(ctlr.GetAcceptResponseRawBytes());
  serialized.set_status(ctlr.ErrorCode());
  if (serialized.using_raw_bytes()) {
    serialized.set_body(FlattenSlow(ctlr.GetResponseRawBytes()));
  } else {
    serialized.set_body(response->SerializeAsString());
  }
  serialized.set_attachment(FlattenSlow(ctlr.GetResponseAttachment()));
  return serialized.SerializeAsString();
}

LoadBalancer::Status RpcStatusToNslbStatus(int rpc_status) {
  if (rpc_status == rpc::STATUS_SUCCESS ||
      rpc_status > rpc::STATUS_RESERVED_MAX /* User error. */) {
    return LoadBalancer::Status::Success;
  } else if (rpc_status == rpc::STATUS_OVERLOADED) {
    return LoadBalancer::Status::Overloaded;
  }
  return LoadBalancer::Status::Failed;
}

}  // namespace

struct RpcChannel::RpcCompletionDesc {
  int status;  // Not significant if `msg` is provided. In this case
               // `msg.meta.response_meta.status` should be used instead.
  protobuf::ProtoMessage* msg = nullptr;

  // Some miscellaneous info. about this RPC.
  const StreamCallGate::Timestamps* timestamps =
      &internal::EarlyInitConstant<StreamCallGate::Timestamps>();
  const Endpoint* remote_peer = &internal::EarlyInitConstant<Endpoint>();
};

struct RpcChannel::Impl {
  // If this, this channel will be used instead. Used for performing RPC mock /
  // dry-run.
  MaybeOwning<google::protobuf::RpcChannel> alternative_channel;

  bool opened = false;
  bool multiplexable;
  std::unique_ptr<MessageDispatcher> message_dispatcher;
  Factory<StreamProtocol> protocol_factory;
  rpc::internal::StreamCallGatePool* call_gate_pool;
};

RpcChannel::RpcChannel() { impl_ = std::make_unique<Impl>(); }

RpcChannel::~RpcChannel() = default;

RpcChannel::RpcChannel(std::string address, const Options& options)
    : RpcChannel() {
  (void)Open(std::move(address), options);  // Failure is ignored.
}

bool RpcChannel::Open(std::string address, const Options& options) {
  options_ = options;
  address_ = address;
  NormalizeUriScheme(&address);

  if (FLARE_UNLIKELY(IsMockAddress(address))) {
    FLARE_CHECK(mock_channel,
                "Mock channel has not been registered yet. Did you forget to "
                "link `flare/testing:rpc_mock`?");
    impl_->alternative_channel =
        std::make_unique<protobuf::detail::MockChannelAdapter>(mock_channel);
    return true;
  }
  if (FLARE_UNLIKELY(rpc::IsDryRunContextPresent())) {
    auto dry_run_channel = std::make_unique<protobuf::RpcChannelForDryRun>();
    if (!dry_run_channel->Open(address_)) {
      return false;
    }
    impl_->alternative_channel = std::move(dry_run_channel);
    return true;
  }

  // Parse URI.
  auto inspection_result = InspectUri(address);
  if (!inspection_result) {
    FLARE_LOG_WARNING_EVERY_SECOND("URI [{}] is not recognized.", address);
    return false;
  }

  // Initialize NSLB, etc.
  auto&& [scheme, addr] = *inspection_result;
  impl_->protocol_factory =
      client_side_stream_protocol_registry.GetFactory(scheme);
  if (!options.override_nslb.empty()) {
    impl_->message_dispatcher =
        NewMessageDispatcherFromName(options.override_nslb);
  } else {
    impl_->message_dispatcher = MakeMessageDispatcher("rpc", address);
  }
  if (!impl_->message_dispatcher || !impl_->message_dispatcher->Open(addr)) {
    FLARE_LOG_WARNING_EVERY_SECOND("URI [{}] is not resolvable.", address);
    return false;
  }
  impl_->call_gate_pool = rpc::internal::GetGlobalStreamCallGatePool(scheme);
  impl_->multiplexable =
      !impl_->protocol_factory()->GetCharacteristics().not_multiplexable;
  impl_->opened = true;

  return true;
}

void RpcChannel::RegisterMockChannel(protobuf::detail::MockChannel* channel) {
  FLARE_CHECK(!mock_channel, "Mock channel has already been registered");
  mock_channel = channel;
}

void RpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                            google::protobuf::RpcController* controller,
                            const google::protobuf::Message* request,
                            google::protobuf::Message* response,
                            google::protobuf::Closure* done) {
  auto ctlr = flare::down_cast<RpcClientController>(controller);
  ctlr->PrecheckForNewRpc();

  bool is_streaming_rpc = protobuf::IsStreamingMethod(method);
  if (is_streaming_rpc) {
    ctlr->SetIsStreaming();
  }

  // Let's see if we're hooked by someone else (e.g. RPC mock).
  if (impl_->alternative_channel) {
    return impl_->alternative_channel->CallMethod(method, controller, request,
                                                  response, done);
  }

  if (!is_streaming_rpc) {
    CallMethodWritingBinlog(method,
                            flare::down_cast<RpcClientController>(controller),
                            request, response, done);
  } else {
    FLARE_LOG_ERROR_IF_ONCE(
        rpc::IsBinlogDumpContextPresent(),
        "RPC binlog is not supported for streaming RPC (yet).");
    FLARE_CHECK_EQ(ctlr->GetMaxRetries(), 1,
                   "Automatic retry is not supported by streaming RPC.");
    FLARE_CHECK(
        !response,
        "`response` shouldn't be provided when making streaming RPC. The "
        "framework (`RpcChannel`, to be precise) has no way to know when "
        "you'll finish writing requests (i.e., client-streaming RPC), "
        "therefore the only meaningful way for you to read the response is via "
        "`StreamReader`, even in the case there's only one response message. "
        "So, use stream given by `RpcClientController` instead.");
    CallStreamingMethod(method, request,
                        flare::down_cast<RpcClientController>(controller),
                        done);
  }
}

void RpcChannel::CallMethodWritingBinlog(
    const google::protobuf::MethodDescriptor* method,
    RpcClientController* controller, const google::protobuf::Message* request,
    google::protobuf::Message* response, google::protobuf::Closure* done) {
  // Let's see if this call should be dumped.
  auto* binlogger = FLARE_UNLIKELY(rpc::IsBinlogDumpContextPresent())
                        ? StartDumpingFor(method, controller)
                        : nullptr;

  if (FLARE_UNLIKELY(binlogger)) {
    // Fake a meta message for binlogger.
    //
    // FIXME: This is weird. But if in case we retries, which RPC meta should we
    // use?
    auto meta = object_pool::Get<rpc::RpcMeta>();
    meta->mutable_request_meta()->mutable_method_name()->assign(
        method->full_name().begin(), method->full_name().end());

    // Notify the framework about this outgoing call.
    binlog::ProtoPacketDesc desc;
    desc.meta = meta.Get();
    if (controller->HasRequestRawBytes()) {
      desc.message = &controller->GetRequestRawBytes();
    } else {
      desc.message = request;
    }
    desc.attachment = &controller->GetRequestAttachment();
    binlogger->AddOutgoingPacket(desc);
  }

  fiber::Latch latch(1);

  auto cb = [this, controller, done, binlogger, response, &latch] {
    if (FLARE_UNLIKELY(binlogger)) {
      auto meta = object_pool::Get<rpc::RpcMeta>();
      meta->mutable_response_meta()->set_status(controller->ErrorCode());

      binlog::ProtoPacketDesc desc;
      desc.meta = meta.Get();
      if (controller->GetAcceptResponseRawBytes()) {
        desc.message = &controller->GetResponseRawBytes();
      } else {
        desc.message = response;
      }
      desc.attachment = &controller->GetResponseAttachment();
      // If calling `WriteBinlogContext` is deemed too slow, we can defer its
      // evaluation by capturing the context and construct a `LazyEval<T>`.
      binlogger->AddIncomingPacket(desc,
                                   WriteBinlogContext(*controller, response));
      FinishDumpingWith(binlogger, controller);
    }
    if (done) {
      done->Run();
    } else {
      latch.count_down();
    }
  };
  auto completion = flare::NewCallback(std::move(cb));

  CallMethodWithRetry(method, controller, request, response, completion,
                      controller->GetMaxRetries());
  if (!done) {  // It was a blocking call.
    latch.wait();
  }
}

void RpcChannel::CallMethodWithRetry(
    const google::protobuf::MethodDescriptor* method,
    RpcClientController* controller, const google::protobuf::Message* request,
    google::protobuf::Message* response, google::protobuf::Closure* done,
    std::size_t retries_left) {
  auto cb = [this, method, controller, request, response, done,
             retries_left](const RpcCompletionDesc& desc) mutable {
    // The RPC has failed and there's still budget for retry, let's retry then.
    if (desc.status != rpc::STATUS_SUCCESS &&
        // Not user error.
        (desc.status != rpc::STATUS_FAILED &&
         desc.status <= rpc::STATUS_RESERVED_MAX) &&
        retries_left != 1) {
      FLARE_CHECK_GT(retries_left, 1);
      CallMethodWithRetry(method, controller, request, response, done,
                          retries_left - 1);
      return;
    }

    // It's the final result.
    controller->SetCompletion(done);
    CopyInterestedFieldsFromMessageToController(desc, controller);

    if (desc.msg) {
      auto&& resp_meta = desc.msg->meta->response_meta();
      controller->NotifyCompletion(
          FLARE_LIKELY(resp_meta.status() == rpc::STATUS_SUCCESS)
              ? Status()
              : Status(resp_meta.status(), resp_meta.description()));
    } else {
      controller->NotifyCompletion(Status(desc.status));
    }
  };
  CallMethodNoRetry(method, request, *controller, response, std::move(cb));
}

template <class F>
void RpcChannel::CallMethodNoRetry(
    const google::protobuf::MethodDescriptor* method,
    const google::protobuf::Message* request,
    const RpcClientController& controller, google::protobuf::Message* response,
    F&& cb) {
  // Find a peer to call.
  std::uintptr_t nslb_ctx;
  Endpoint remote_peer;
  if (FLARE_UNLIKELY(!GetPeerOrFailEarlyForFastCall(*method, &remote_peer,
                                                    &nslb_ctx, cb))) {
    return;
  }

  // Describe several aspect of this RPC.
  auto call_ctx = object_pool::Get<protobuf::ProactiveCallContext>();
  auto call_ctx_ptr = call_ctx.Get();  // Used later.
  call_ctx->accept_response_in_bytes = controller.GetAcceptResponseRawBytes();
  call_ctx->expecting_stream = false;
  call_ctx->method = method;
  call_ctx->response_ptr = response;

  // Open a gate and keep an extra ref. on it.
  //
  // The extra ref. is required to keep the gate alive until `FastCall`
  // finishes. This is necessary to prevent the case when our callback is called
  // (therefore, `ctlr.DetachGate()` is called) before `FastCall` return, we
  // need to keep a ref-count ourselves.
  auto gate_handle = GetFastCallGate(remote_peer);
  RefPtr gate_ptr{ref_ptr, gate_handle.Get()};

  // Now that we know who would serve us, create a span for tracing this RPC and
  // pass it down.
  auto tracing_span = StartTracingSpanFor(gate_ptr->GetEndpoint(), method);
  if (FLARE_UNLIKELY(tracing_span.Tracing())) {
    tracing_span.WriteContextTo(call_ctx->MutableTracingContext());
  }

  // Context passed to our completion callback.
  auto cb_ctx = object_pool::Get<FastCallContext>();
  cb_ctx->nslb_ctx = nslb_ctx;
  cb_ctx->call_ctx = std::move(call_ctx);
  cb_ctx->call_gate_handle = std::move(gate_handle);
  cb_ctx->tracing_span = std::move(tracing_span);
  cb_ctx->multiplexable = impl_->multiplexable;

  // Completion callback.
  auto on_completion = [this, cb_ctx = std::move(cb_ctx), cb = std::move(cb)](
                           auto status, auto msg_ptr,
                           auto&& timestamps) mutable {
    Endpoint remote_peer = cb_ctx->call_gate_handle->GetEndpoint();

    // The RPC timed out. Besides, the connection does not support multiplexing.
    //
    // In this case we must close the connection to avoid confusion in
    // correspondence between subsequent request and pending responses (to this
    // one, and to newer request).
    if (!msg_ptr && !cb_ctx->multiplexable) {
      cb_ctx->call_gate_handle->SetUnhealthy();
    }
    cb_ctx->call_gate_handle.Close();

    auto proto_msg = cast_or_null<protobuf::ProtoMessage>(msg_ptr.get());
    int rpc_status = proto_msg ? proto_msg->meta->response_meta().status()
                               : TranslateRpcError(status);

    // We'll report this span whether or not it'll be retried. If it's
    // retried, it's another span.
    if (FLARE_UNLIKELY(rpc::IsTracedContextPresent())) {
      FinishTracingSpanWith(rpc_status, &cb_ctx->tracing_span,
                            cb_ctx->call_ctx->IsTraceForciblySampled());
    }

    impl_->message_dispatcher->Report(
        remote_peer, GetLoadBalancerFeedbackStatusFrom(rpc_status),
        DurationFromTsc(timestamps.sent_tsc, timestamps.received_tsc),
        std::exchange(cb_ctx->nslb_ctx, 0));

    cb(RpcCompletionDesc{.status = rpc_status,
                         .msg = proto_msg,
                         .timestamps = &timestamps,
                         .remote_peer = &remote_peer});
  };

  // Prepare the request message.
  protobuf::ProtoMessage req_msg;
  CreateNativeRequestForFastCall(*method, request, controller, &req_msg);

  // And issue the call.
  auto args = object_pool::Get<StreamCallGate::FastCallArgs>();
  args->completion = std::move(on_completion);
  args->controller = call_ctx_ptr;
  if (auto ptr = fiber::ExecutionContext::Current(); FLARE_LIKELY(ptr)) {
    args->exec_ctx.Reset(ref_ptr, ptr);
  }
  gate_ptr->FastCall(req_msg, std::move(args), controller.GetTimeout());
}

void RpcChannel::CallStreamingMethod(
    const google::protobuf::MethodDescriptor* method,
    const google::protobuf::Message* request, RpcClientController* controller,
    google::protobuf::Closure* done) {
  controller->InitializeStreamingRpcContext();

  std::uintptr_t nslb_ctx;
  Endpoint remote_peer;

  // Bail out early if any error occurred.
  bool early_failure = false;
  if (FLARE_UNLIKELY(!impl_->opened)) {
    FLARE_LOG_WARNING_EVERY_SECOND(
        "Calling method [{}] on failed channel [{}].", method->full_name(),
        address_);
    early_failure = true;
  }
  if (FLARE_UNLIKELY(!impl_->message_dispatcher->GetPeer(
          GetNextPseudoRandomKey(), &remote_peer, &nslb_ctx))) {
    FLARE_LOG_WARNING_EVERY_SECOND(
        "No peer available for calling method [{}] on [{}].",
        method->service()->full_name(), address_);
    early_failure = true;
  }
  if (FLARE_UNLIKELY(early_failure)) {
    // TODO(luobogao): UT.
    auto&& [i, o] = GetErrorStreams();
    controller->SetStream(std::move(i), std::move(o));
    controller->SetCompletion(flare::NewCallback([done] {
      if (done) {
        done->Run();
      }
    }));
    return;
  }

  auto gate_handle = GetStreamCallGate(remote_peer);
  RefPtr gate_ptr{ref_ptr, gate_handle.Get()};  // See comments for fast call.
  auto correlation_id = NextCorrelationId();
  FLARE_CHECK(controller->GetRequestAttachment().Empty(),
              "Attachment is not supported in streaming RPC.");
  FLARE_CHECK(!controller->GetAcceptResponseRawBytes() &&
                  !controller->HasRequestRawBytes(),
              "Accepting response in bytes is not supported in streaming RPC.");

  if (gate_ptr->GetProtocol().GetCharacteristics().no_end_of_stream_marker) {
    controller->DisableEndOfStreamMarker();
  }

  controller->SetCompletion(flare::NewCallback([this, remote_peer, done,
                                                controller, nslb_ctx] {
    // FIXME: UGLY HACK.
    //
    // We can't free the handle here as `Join`-ing the handle would wait until
    // all stream callback to finish. As we're freeing the handle in stream
    // callback, this will obviously lead to deadlock.
    //
    // I do think we need to refactor the whole streaming RPC design.
    auto handle = std::move(controller->GetStreamingRpcContext()->call_gate);
    fiber::internal::StartFiberDetached([handle = std::move(handle)] {});

    // Report NSLB result.
    auto nslb_status = RpcStatusToNslbStatus(controller->ErrorCode());
    impl_->message_dispatcher->Report(
        remote_peer, nslb_status, {} /* time_cost, does not applicable here. */,
        nslb_ctx);
    if (done) {  // For streaming calls, there's hardly a point in using `done`.
      done->Run();
    }
  }));
  controller->SetRemotePeer(gate_ptr->GetEndpoint());

  // Initialize streaming RPC context..
  auto&& streaming_rpc_ctx = controller->GetStreamingRpcContext();
  streaming_rpc_ctx->call_gate = std::move(gate_handle);

  auto call_ctx = &streaming_rpc_ctx->call_ctx;
  call_ctx->accept_response_in_bytes = false;
  call_ctx->expecting_stream = true;
  call_ctx->method = method;
  call_ctx->response_prototype =
      google::protobuf::MessageFactory::generated_factory()->GetPrototype(
          method->output_type());

  auto meta = object_pool::Get<rpc::RpcMeta>();
  meta->set_correlation_id(correlation_id);
  meta->set_method_type(rpc::METHOD_TYPE_STREAM);
  meta->mutable_request_meta()->set_method_name(method->full_name());
  meta->mutable_request_meta()->set_timeout(controller->GetRelativeTimeout() /
                                            1ms);
  // `type` is filled by `RpcClientController` itself.
  controller->SetRpcMetaPrototype(*meta);

  // TODO(luobogao): Packing tracing information to streaming RPCs. (But how
  // should we do this? Should we attach a tracing context into each message, or
  // only the first one?)
  if (rpc::IsTracedContextPresent()) {
    FLARE_LOG_ERROR_ONCE(
        "Not implemented: Distributed tracing for streaming RPC is not "
        "implemented yet.");
  }

  auto&& [is, os] = gate_ptr->StreamCall(correlation_id, call_ctx);
  if (!method->client_streaming()) {
    auto req_msg = std::make_unique<protobuf::ProtoMessage>();

    req_msg->meta = object_pool::Get<rpc::RpcMeta>();
    req_msg->meta->set_correlation_id(correlation_id);
    req_msg->meta->set_method_type(rpc::METHOD_TYPE_STREAM);
    req_msg->meta->set_flags(rpc::MESSAGE_FLAGS_START_OF_STREAM |
                             rpc::MESSAGE_FLAGS_END_OF_STREAM);
    req_msg->meta->mutable_request_meta()->mutable_method_name()->assign(
        method->full_name().begin(), method->full_name().end());
    req_msg->msg_or_buffer = MaybeOwning(non_owning, request);

    // Blocking may occur here if the connection fails before our data is
    // actually written out, so we apply a timeout here.
    os.SetExpiration(controller->GetStreamTimeout());
    bool success = fiber::BlockingGet(os.WriteLast(std::move(req_msg)));
    if (success) {
      controller->SetStreamReader(std::move(is));
    } else {
      // Given that the request was not successful written, there's no point in
      // using the response reader.
      //
      // So we close the response reader and fake a "always error" stream to the
      // user.
      is.Close();
      controller->SetStreamReader(AsyncStreamReader<std::unique_ptr<Message>>(
          MakeRefCounted<rpc::detail::ErrorStreamReaderProvider<
              std::unique_ptr<Message>>>()));
    }
  } else {
    controller->SetStream(std::move(is), std::move(os));
    // Nothing to write. It's up to the user to write something into the streams
    // (accessible via `RpcClientController`.).
  }
}

template <class F>
bool RpcChannel::GetPeerOrFailEarlyForFastCall(
    const google::protobuf::MethodDescriptor& method, Endpoint* peer,
    std::uintptr_t* nslb_ctx, F&& cb) {
  if (FLARE_UNLIKELY(!impl_->opened)) {
    FLARE_LOG_WARNING_EVERY_SECOND(
        "Calling method [{}] on failed channel [{}].", method.full_name(),
        address_);
    cb(RpcCompletionDesc{.status = rpc::STATUS_INVALID_CHANNEL});
    return false;
  }
  if (FLARE_UNLIKELY(!impl_->message_dispatcher->GetPeer(
          GetNextPseudoRandomKey(), peer, nslb_ctx))) {
    FLARE_LOG_WARNING_EVERY_SECOND(
        "No peer available for calling method [{}] on [{}].",
        method.full_name(), address_);
    cb(RpcCompletionDesc{.status = rpc::STATUS_NO_PEER});
    return false;
  }

  return true;
}

void RpcChannel::CreateNativeRequestForFastCall(
    const google::protobuf::MethodDescriptor& method,
    const google::protobuf::Message* request,
    const RpcClientController& controller, protobuf::ProtoMessage* to) {
  static constexpr std::uint64_t kAcceptableCompressionAlgorithms =
      1 << rpc::COMPRESSION_ALGORITHM_NONE |
      1 << rpc::COMPRESSION_ALGORITHM_GZIP |
      1 << rpc::COMPRESSION_ALGORITHM_LZ4_FRAME |
      1 << rpc::COMPRESSION_ALGORITHM_SNAPPY |
      1 << rpc::COMPRESSION_ALGORITHM_ZSTD;

  // Initialize meta.
  auto meta = object_pool::Get<rpc::RpcMeta>();
  meta->set_correlation_id(NextCorrelationId());
  meta->set_method_type(rpc::METHOD_TYPE_SINGLE);
  meta->mutable_request_meta()->mutable_method_name()->assign(
      method.full_name().begin(), method.full_name().end());
  meta->mutable_request_meta()->set_timeout(controller.GetRelativeTimeout() /
                                            1ms);
  meta->mutable_request_meta()->set_acceptable_compression_algorithms(
      kAcceptableCompressionAlgorithms);
  if (auto compression_algorithm = controller.GetCompressionAlgorithm();
      compression_algorithm != rpc::COMPRESSION_ALGORITHM_NONE) {
    meta->set_compression_algorithm(compression_algorithm);
    meta->set_attachment_compressed(true);
  }
  to->meta = std::move(meta);

  // Initialize body.
  if (FLARE_UNLIKELY(controller.HasRequestRawBytes())) {
    EnsureBytesOfInputTypeInDebugMode(&method, controller.GetRequestRawBytes());
    to->msg_or_buffer = controller.GetRequestRawBytes();
  } else {
    to->msg_or_buffer = MaybeOwning(non_owning, request);
  }

  // And (optionally) the attachment.
  to->attachment = controller.GetRequestAttachment();
}

inline std::uint32_t RpcChannel::NextCorrelationId() const noexcept {
  if (FLARE_LIKELY(impl_->multiplexable)) {
    return rpc::internal::NewRpcCorrelationId();
  } else {
    return Message::kNonmultiplexableCorrelationId;
  }
}

tracing::QuickerSpan RpcChannel::StartTracingSpanFor(
    const Endpoint& peer, const google::protobuf::MethodDescriptor* method) {
  if (!rpc::IsTracedContextPresent()) {
    return {};  // Do not trace this call then.
  }

  // Start a new span for this RPC.
  auto span =
      rpc::session_context->tracing.tracing_ops->StartSpanWithLazyOptions(
          // As suggested by OpenTracing standard, we use fully-qualified method
          // name here.
          method->full_name(), [&](auto&& f) {
            f(opentracing::ChildOf(
                rpc::session_context->tracing.server_span.SpanContext()));
          });

  // Tags are set separately for better performance.
  span.SetStandardTag(opentracing::ext::span_kind,
                      opentracing::ext::span_kind_rpc_client);
  span.SetStandardTag(opentracing::ext::peer_service,
                      method->service()->full_name().c_str());
  span.SetStandardTag(peer.Family() == AF_INET
                          ? opentracing::ext::peer_host_ipv4
                          : opentracing::ext::peer_host_ipv6,
                      [peer] { return EndpointGetIp(peer); });
  span.SetStandardTag(opentracing::ext::peer_port, EndpointGetPort(peer));
  return span;
}

void RpcChannel::FinishTracingSpanWith(int completion_status,
                                       tracing::QuickerSpan* span,
                                       bool forcibly_sampled) {
  if (rpc::IsTracedContextPresent()) {
    if (forcibly_sampled) {
      span->SetForciblySampled();
    } else if (completion_status != rpc::STATUS_SUCCESS) {
      span->AdviseForciblySampled();
    }
    if (span->IsForciblySampled()) {
      rpc::session_context->tracing.server_span.SetForciblySampled();
    }
  }
  span->SetFrameworkTag(tracing::ext::kInvocationStatus, completion_status);
  // `opentracing::ext::error` is not set to avoid TJG's poor implementation.
  // FIXME: What about other tracing providers then?
  span->Report();
}

binlog::OutgoingCallWriter* RpcChannel::StartDumpingFor(
    const google::protobuf::MethodDescriptor* method,
    RpcClientController* ctlr) {
  FLARE_CHECK(rpc::IsBinlogDumpContextPresent());

  auto outgoing = rpc::session_context->binlog.dumper->StartOutgoingCall();
  if (!outgoing) {  // It's explicitly allowed to return `nullptr` is the
                    // implementation is not interested in capturing outgoing
                    // calls.
    return nullptr;
  }

  outgoing->SetCorrelationId(GetBinlogCorrelationId(method, *ctlr));
  outgoing->SetOperationName(method->full_name());
  outgoing->SetStartTimestamp(ReadSteadyClock());
  outgoing->SetUri(address_);

  return outgoing;
}

void RpcChannel::FinishDumpingWith(binlog::OutgoingCallWriter* logger,
                                   RpcClientController* ctlr) {
  FLARE_CHECK(logger);
  logger->SetInvocationStatus(Format("{}", ctlr->ErrorCode()));
  logger->SetFinishTimestamp(ReadSteadyClock());
}

std::string RpcChannel::GetBinlogCorrelationId(
    const google::protobuf::MethodDescriptor* method,
    const RpcClientController& ctlr) {
  FLARE_CHECK(rpc::IsBinlogDumpContextPresent());
  return Format("rpc-{}-{}-{}-{}", rpc::session_context->binlog.correlation_id,
                method->full_name(), address_, ctlr.GetBinlogCorrelationId());
}

void RpcChannel::CopyInterestedFieldsFromMessageToController(
    const RpcCompletionDesc& completion_desc, RpcClientController* ctlr) {
  ctlr->SetRemotePeer(*completion_desc.remote_peer);
  ctlr->SetTimestamp(RpcClientController::Timestamp::Sent,
                     completion_desc.timestamps->sent_tsc);
  ctlr->SetTimestamp(RpcClientController::Timestamp::Received,
                     completion_desc.timestamps->received_tsc);
  ctlr->SetTimestamp(RpcClientController::Timestamp::Parsed,
                     completion_desc.timestamps->parsed_tsc);
  if (auto msg = completion_desc.msg) {
    ctlr->SetResponseAttachment(msg->attachment);
    if (ctlr->GetAcceptResponseRawBytes()) {
      if (msg->msg_or_buffer.index() == 2) {
        ctlr->SetResponseRawBytes(std::move(std::get<2>(msg->msg_or_buffer)));
      } else {
        FLARE_CHECK_EQ(0, msg->msg_or_buffer.index());
        // We still have to initialize response bytes even though it's
        // empty. Otherwise calling `GetResponseRawBytes` would report it's
        // not initialized.
        ctlr->SetResponseRawBytes({});
      }
    }
  }
}

rpc::internal::StreamCallGateHandle RpcChannel::GetFastCallGate(
    const Endpoint& ep) {
  if (impl_->multiplexable) {
    return impl_->call_gate_pool->GetOrCreateShared(
        ep, false, [&] { return CreateCallGate(ep); });
  } else {
    return impl_->call_gate_pool->GetOrCreateExclusive(
        ep, [&] { return CreateCallGate(ep); });
  }
}

rpc::internal::StreamCallGateHandle RpcChannel::GetStreamCallGate(
    const Endpoint& ep) {
  // We always use dedicated connection for streaming RPC to avoid HOL blocking.

  // FIXME: Even if we tested `Healthy()` after `GetOrCreate`, there's still a
  // time window between we test it and we use it, we'd better fix this in
  // `Channel` by retry on write failure. (When `Write()` returns `false`, the
  // message is not sent, therefore it can be safely retried.)
  //
  // But what about streaming calls?
  while (true) {
    // We unconditionally use dedicated connection for stream calls if
    // `flare_rpc_streaming_rpc_dedicated_connection` is not set. Overhead of
    // establishing connection should be eligible for stream calls (if it
    // streams a lot.)
    //
    // OTOH, had we used exclusive connection here, we need to balancing
    // connections between fast calls and stream calls, which is rather nasty.
    auto rc = impl_->call_gate_pool->GetOrCreateDedicated(
        [&] { return CreateCallGate(ep); });
    if (!rc->Healthy()) {
      rc.Close();
      continue;
    }
    return rc;
  }
}

RefPtr<rpc::internal::StreamCallGate> RpcChannel::CreateCallGate(
    const Endpoint& ep) {
  auto gate = MakeRefCounted<rpc::internal::StreamCallGate>();
  StreamCallGate::Options opts;
  opts.protocol = impl_->protocol_factory();
  opts.maximum_packet_size = options_.maximum_packet_size;
  gate->Open(ep, std::move(opts));
  if (!gate->Healthy()) {
    FLARE_LOG_WARNING_EVERY_SECOND("Failed to open new call gate to [{}].",
                                   ep.ToString());
    // Fall-through. We don't want to handle failure in any special way.
  }
  return gate;
}

}  // namespace flare

namespace flare {

template <>
struct PoolTraits<FastCallContext> {
  static constexpr auto kType = PoolType::MemoryNodeShared;
  static constexpr auto kLowWaterMark = 8192;
  static constexpr auto kHighWaterMark =
      std::numeric_limits<std::size_t>::max();
  static constexpr auto kMaxIdle = std::chrono::seconds(10);
  static constexpr auto kMinimumThreadCacheSize = 1024;
  static constexpr auto kTransferBatchSize = 1024;

  static void OnPut(FastCallContext* ptr) {
    FLARE_CHECK_EQ(ptr->nslb_ctx, 0);
    ptr->call_ctx = nullptr;
    ptr->call_gate_handle.Close();
    FLARE_CHECK(!ptr->tracing_span.Tracing());
  }
};

}  // namespace flare
