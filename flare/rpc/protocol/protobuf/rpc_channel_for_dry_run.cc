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

#define FLARE_RPC_CLIENT_CONTROLLER_SUPPRESS_INCLUDE_WARNING

#include "flare/rpc/protocol/protobuf/rpc_channel_for_dry_run.h"

#include "flare/base/callback.h"
#include "flare/base/chrono.h"
#include "flare/base/down_cast.h"
#include "flare/base/string.h"
#include "flare/fiber/future.h"
#include "flare/fiber/timer.h"
#include "flare/rpc/internal/fast_latch.h"
#include "flare/rpc/internal/session_context.h"
#include "flare/rpc/protocol/protobuf/binlog.pb.h"
#include "flare/rpc/protocol/protobuf/rpc_client_controller.h"
#include "flare/rpc/protocol/protobuf/rpc_meta.pb.h"

namespace flare::protobuf {

namespace {

// @sa: RpcChannel::GetBinlogCorrelationId.
std::string GetBinlogCorrelationId(
    const std::string& channel_uri, const std::string& method_full_name,
    const std::string& ctlr_binlog_correlation_id) {
  return Format("rpc-{}-{}-{}-{}", rpc::session_context->binlog.correlation_id,
                method_full_name, channel_uri, ctlr_binlog_correlation_id);
}

}  // namespace

bool RpcChannelForDryRun::Open(const std::string& uri) {
  uri_ = uri;
  return true;
}

void RpcChannelForDryRun::CallMethod(
    const google::protobuf::MethodDescriptor* method,
    google::protobuf::RpcController* controller,
    const google::protobuf::Message* request,
    google::protobuf::Message* response, google::protobuf::Closure* done) {
  FLARE_CHECK(rpc::session_context->binlog.dry_runner);

  rpc::detail::FastLatch fast_latch;
  internal::LocalCallback done_callback([&] { fast_latch.count_down(); });

  auto ctlr = flare::down_cast<RpcClientController>(controller);
  FLARE_CHECK(
      !ctlr->IsStreaming(),
      "Not implemented: Support for streaming RPC when performing dry run.");
  FLARE_CHECK(
      !ctlr->HasRequestRawBytes() && !ctlr->GetAcceptResponseRawBytes(),
      "Making request by raw bytes is not supported when performing dry-run.");
  auto cid = GetBinlogCorrelationId(uri_, method->full_name(),
                                    ctlr->GetBinlogCorrelationId());

  ctlr->SetCompletion(done ? done : &done_callback);
  auto cb = [=](Expected<binlog::DryRunPacket, Status> packet) {
    if (!packet) {
      FLARE_LOG_WARNING_EVERY_SECOND("`GetIncomingPacket` failed with: {}",
                                     packet.error().ToString());
      ctlr->NotifyCompletion(Status(rpc::STATUS_FAILED));
      return;
    }

    rpc::SerializedClientPacket result;
    if (!result.ParseFromString(packet->system_ctx)) {
      FLARE_LOG_ERROR_EVERY_SECOND(
          "Unexpected: Failed to parse `OutgoingCall.context`. Incompatible "
          "binlog replayed?");
      ctlr->NotifyCompletion(Status(rpc::STATUS_FAILED));
      return;
    }

    if (result.status() != rpc::STATUS_SUCCESS) {
      ctlr->NotifyCompletion(Status(result.status()));
      return;
    }
    if (result.using_raw_bytes()) {
      ctlr->SetResponseRawBytes(CreateBufferSlow(result.body()));
    } else {
      if (!response->ParseFromString(result.body())) {
        FLARE_LOG_WARNING_EVERY_SECOND("Failed to parse response body as [{}].",
                                       response->GetDescriptor()->full_name());
        ctlr->NotifyCompletion(Status(rpc::STATUS_MALFORMED_DATA));
        return;
      }
    }
    if (!result.attachment().empty()) {
      ctlr->SetResponseAttachment(CreateBufferSlow(result.attachment()));
    }
    ctlr->NotifyCompletion(Status(rpc::STATUS_SUCCESS));
  };
  rpc::RpcMeta meta;
  meta.set_correlation_id(0);  // Does not matter, I think.
  meta.set_method_type(rpc::METHOD_TYPE_STREAM);
  meta.mutable_request_meta()->set_method_name(method->full_name());

  auto call =
      rpc::session_context->binlog.dry_runner->TryStartOutgoingCall(cid);
  if (!call) {
    FLARE_LOG_WARNING_EVERY_SECOND(
        "Unexpected RPC. Are you making calls to a new backend?");
    ctlr->NotifyCompletion(Status(rpc::STATUS_FAILED));
    // Fall-through.
  } else {
    (*call)->CaptureOutgoingPacket(
        binlog::ProtoPacketDesc(meta, *request, ctlr->GetRequestAttachment()));

    // Streaming RPC is not supported yet.
    (*call)
        ->TryGetIncomingPacketEnumlatingDelay(0 /* First response */)
        .Then(std::move(cb));
  }

  if (!done) {
    fast_latch.wait();
  }
}

}  // namespace flare::protobuf
