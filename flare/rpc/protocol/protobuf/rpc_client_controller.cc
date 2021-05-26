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

#include "flare/rpc/protocol/protobuf/rpc_client_controller.h"

#include <chrono>
#include <string>

#include "thirdparty/gflags/gflags.h"

#include "flare/base/logging.h"
#include "flare/rpc/protocol/protobuf/call_context.h"

using namespace std::literals;

DEFINE_int32(flare_rpc_client_default_rpc_timeout_ms, 2000,
             "Default RPC timeout for non-streaming RPCs.");

namespace flare {

bool RpcClientController::Failed() const {
  FLARE_CHECK(
      completed_,
      "Calling `Failed()` before RPC has completed makes no sense. If you see "
      "this error in UT, it's likely your RPC mock does not work correctly.");
  return !rpc_status_ || !rpc_status_->ok();
}

int RpcClientController::ErrorCode() const {
  return rpc_status_ ? rpc_status_->code()
                     : static_cast<int>(rpc::STATUS_FAILED);
}

std::string RpcClientController::ErrorText() const {
  if (!rpc_status_) {
    return "(unknown failure)";
  }
  return rpc_status_->message();
}

void RpcClientController::SetTimeout(
    internal::SteadyClockView timeout) noexcept {
  timeout_ = timeout.Get();
  SetStreamTimeout(timeout_);  // TODO(luobogao): Remove this.
}

std::chrono::steady_clock::time_point RpcClientController::GetTimeout()
    const noexcept {
  return timeout_;
}

void RpcClientController::SetMaxRetries(std::size_t max_retries) {
  max_retries_ = max_retries;
}

std::size_t RpcClientController::GetMaxRetries() const { return max_retries_; }

void RpcClientController::SetAcceptResponseRawBytes(bool f) noexcept {
  accept_resp_in_bytes_ = f;
}

bool RpcClientController::GetAcceptResponseRawBytes() const noexcept {
  return accept_resp_in_bytes_;
}

void RpcClientController::SetFailed(const std::string& reason) {
  FLARE_CHECK(0, "Unexpected.");
}

std::chrono::steady_clock::time_point RpcClientController::GetTimestampSent() {
  return RpcControllerCommon::GetTimestamp(Timestamp::Sent);
}

std::chrono::steady_clock::time_point
RpcClientController::GetTimestampReceived() {
  return RpcControllerCommon::GetTimestamp(Timestamp::Received);
}

std::chrono::steady_clock::time_point
RpcClientController::GetTimestampParsed() {
  return RpcControllerCommon::GetTimestamp(Timestamp::Parsed);
}

void RpcClientController::Reset() {
  RpcControllerCommon::Reset();
  in_use_ = false;
  completed_ = false;

  max_retries_ = 1;
  last_reset_ = ReadSteadyClock();
  timeout_ = last_reset_ + 1ms * FLAGS_flare_rpc_client_default_rpc_timeout_ms;
  accept_resp_in_bytes_ = false;
  completion_ = nullptr;
  compression_algorithm_ = rpc::COMPRESSION_ALGORITHM_NONE;

  rpc_status_ = std::nullopt;
  binlog_correlation_id_ = "";

  streaming_rpc_ctx_.reset();
}

void RpcClientController::PrecheckForNewRpc() {
  FLARE_LOG_ERROR_IF_EVERY_SECOND(
      in_use_,
      "UNDEFINED BEHAVIOR: You must `Reset()` the `RpcClientController` before "
      "reusing it. THIS ERROR WILL BE RAISED TO A CHECK FAILURE (CRASHING THE "
      "PROGRAM) SOON.");
  FLARE_DCHECK(!in_use_);
  in_use_ = true;
}

void RpcClientController::InitializeStreamingRpcContext() {
  streaming_rpc_ctx_ = std::make_unique<StreamingRpcContext>();
}

RpcClientController::StreamingRpcContext*
RpcClientController::GetStreamingRpcContext() {
  return streaming_rpc_ctx_.get();
}

void RpcClientController::SetCompletion(google::protobuf::Closure* done) {
  completion_ = std::move(done);
}

void RpcClientController::NotifyCompletion(Status status) {
  rpc_status_ = status;

  completed_ = true;
  completion_->Run();

  // Do NOT touch this controller hereafter, as it could have been destroyed in
  // user's completion callback.
}

void RpcClientController::NotifyStreamProgress(const rpc::RpcMeta& meta) {
  FLARE_CHECK(IsStreaming());
  rpc_status_ =
      Status(meta.response_meta().status(), meta.response_meta().description());
}

void RpcClientController::NotifyStreamCompletion(bool reached_eos) {
  FLARE_CHECK(IsStreaming());
  if (!reached_eos) {
    rpc_status_.reset();  // Fatal then.
  }
  completed_ = true;
  if (completion_) {
    completion_->Run();
  }
}

}  // namespace flare
