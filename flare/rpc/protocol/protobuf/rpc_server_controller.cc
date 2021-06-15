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

#define FLARE_RPC_SERVER_CONTROLLER_SUPPRESS_INCLUDE_WARNING

#include "flare/rpc/protocol/protobuf/rpc_server_controller.h"

#include <optional>
#include <string>

#include "google/protobuf/empty.pb.h"

#include "flare/base/logging.h"

namespace flare {

void RpcServerController::SetFailed(const std::string& reason) {
  return SetFailed(rpc::STATUS_FAILED, reason);
}

void RpcServerController::SetFailed(int status, std::string reason) {
  FLARE_CHECK_NE(status, rpc::STATUS_SUCCESS,
                 "You should never call `SetFailed` with `STATUS_SUCCESS`.");
  // I think we should use negative status code to represent severe errors (and
  // therefore, should be reported to NSLB.).
  //
  // Both framework-related error, and critical user error (e.g., inconsistent
  // data) can use negative errors.
  FLARE_CHECK_GE(status, 0, "Negative status codes are reserved.");
  FLARE_LOG_ERROR_IF_ONCE(
      // `STATUS_FAILED` is explicitly allowed for user to use.
      status <= rpc::STATUS_RESERVED_MAX && status != rpc::STATUS_FAILED,
      "`status` in range [0, 1000] is reserved by the framework. You should "
      "always call `SetFailed` with a status code greater than 1000.");
  error_code_ = status;
  error_text_ = std::move(reason);

  // FIXME: We need some refactor here.
  if (IsStreamReaderUntouched()) {
    // Not quite right TBH. It should work, though.
    GetStreamReader<google::protobuf::Empty>().Close();
  }
  if (IsStreamWriterUntouched()) {
    // Not quite right either.
    GetStreamWriter<google::protobuf::Empty>().Close();
  }
}

void RpcServerController::AddTracingLog(std::string value) {
  std::scoped_lock _(user_fields_lock_);
  tracing_user_logs_.emplace_back("", std::move(value));
}

void RpcServerController::SetTracingTag(std::string key, std::string value) {
  std::scoped_lock _(user_fields_lock_);
  tracing_user_tags_.emplace_back(std::move(key), std::move(value));
}

void RpcServerController::SetBinlogTag(std::string key, std::string value) {
  FLARE_CHECK(!InDryRunEnvironment(),
              "`SetBinlogTag is only usable in non-dry-run environment.");
  if (FLARE_UNLIKELY(IsCapturingBinlog())) {
    std::scoped_lock _(user_fields_lock_);
    binlog_user_tags_for_write_.emplace_back(std::move(key), std::move(value));
  }
}

void RpcServerController::AbortBinlogCapture() noexcept {
  is_capturing_binlog_.store(false, std::memory_order_relaxed);
}

std::optional<std::string> RpcServerController::GetBinlogTag(
    const std::string& key) {
  FLARE_CHECK(InDryRunEnvironment(),
              "`GetBinlogTag is only usable in dry-run environment.");
  std::scoped_lock _(user_fields_lock_);  // Not necessarily.
  auto iter = binlog_user_tags_for_read_.find(key);
  if (iter != binlog_user_tags_for_read_.end()) {
    return iter->second;
  }
  return std::nullopt;
}

void RpcServerController::Reset() {
  RpcControllerCommon::Reset();

  error_code_ = rpc::STATUS_SUCCESS;
  timeout_from_caller_ = std::nullopt;
  dry_run_env_ = false;
  comp_algo_ = rpc::COMPRESSION_ALGORITHM_NONE;
  early_write_resp_cb_ = nullptr;
  error_text_.clear();
  acceptable_comp_algos_ = 0;
  resp_attachment_precompressed_ = false;
  tracing_user_logs_.clear();
  tracing_user_tags_.clear();
  binlog_user_tags_for_write_.clear();
  is_capturing_binlog_.store(false, std::memory_order_relaxed);
  binlog_correlation_id_.clear();
  binlog_user_tags_for_read_.clear();
}

bool RpcServerController::Failed() const {
  return error_code_ != rpc::STATUS_SUCCESS;
}

int RpcServerController::ErrorCode() const { return error_code_; }

std::string RpcServerController::ErrorText() const { return error_text_; }

std::bitset<64> RpcServerController::GetAcceptableCompressionAlgorithms()
    const noexcept {
  return acceptable_comp_algos_ | (1 << rpc::COMPRESSION_ALGORITHM_NONE);
}

rpc::CompressionAlgorithm
RpcServerController::GetPreferredCompressionAlgorithm() const noexcept {
  for (auto&& algo :
       {rpc::COMPRESSION_ALGORITHM_ZSTD, rpc::COMPRESSION_ALGORITHM_LZ4_FRAME,
        rpc::COMPRESSION_ALGORITHM_GZIP, rpc::COMPRESSION_ALGORITHM_SNAPPY,
        rpc::COMPRESSION_ALGORITHM_NONE}) {
    if (GetAcceptableCompressionAlgorithms()[algo]) {
      return algo;
    }
  }
  FLARE_UNREACHABLE();
}

void RpcServerController::SetCompressionAlgorithm(
    rpc::CompressionAlgorithm algorithm) {
  comp_algo_ = algorithm;
}

rpc::CompressionAlgorithm RpcServerController::GetCompressionAlgorithm()
    const noexcept {
  return comp_algo_;
}

void RpcServerController::SetResponseAttachmentPrecompressed(bool compressed) {
  resp_attachment_precompressed_ = compressed;
}

bool RpcServerController::GetResponseAttachmentPrecompressed() const noexcept {
  return resp_attachment_precompressed_;
}

}  // namespace flare
