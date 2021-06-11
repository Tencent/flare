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

#ifndef FLARE_RPC_PROTOCOL_PROTOBUF_RPC_SERVER_CONTROLLER_H_
#define FLARE_RPC_PROTOCOL_PROTOBUF_RPC_SERVER_CONTROLLER_H_

#ifndef FLARE_RPC_SERVER_CONTROLLER_SUPPRESS_INCLUDE_WARNING
#warning Use `flare/rpc/rpc_server_controller.h` instead.
#endif

#include <bitset>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "googletest/gtest/gtest_prod.h"

#include "flare/base/string.h"
#include "flare/rpc/protocol/protobuf/rpc_controller_common.h"

namespace flare {

namespace protobuf {

class Service;
class PassiveCallContextFactory;

}  // namespace protobuf

// This controller controls a single RPC. It's used on server-side.
class RpcServerController : public protobuf::RpcControllerCommon {
 public:
  RpcServerController() : protobuf::RpcControllerCommon(true) {}

  // Mark this RPC failed with a reason.
  //
  // Note that `status` no greater than 1000 are reserved for framework. You
  // should always choose your status code in [1001, INT32_MAX]. For the sake of
  // convenience, you're allowed to use `STATUS_FAILED` here, even if it's
  // defined by the framework.
  void SetFailed(const std::string& reason) override;  // Uses `STATUS_FAILED`.
  void SetFailed(int status, std::string reason = "");

  // Returns `true` if `SetFailed` was called.
  bool Failed() const override;

  // Returns whatever was set by `SetFailed`, or `STATUS_SUCCESS` if `SetFailed`
  // was never called. (Perhaps we should call it `GetStatus()` instead.)
  int ErrorCode() const override;  // @sa: `Status` in `rpc_meta.proto`.

  // Returns whatever was given to `SetFailed`.
  std::string ErrorText() const override;

  // If you're really care about responsiveness, once you have finished filling
  // out response message (but before cleaning up things in service method
  // implementation), you can call this method to write response immediately.
  //
  // ONCE THIS METHOD IS CALLED, TOUCHING RESPONSE OR CONTROLLER IN ANY FASHION
  // RESULTS IN UNDEFINED BEHAVIOR.
  //
  // This method can be called at most once. It's not applicable to streaming
  // RPCs.
  void WriteResponseImmediately();

  // If the caller specified a timeout, it can be retrieved here.
  //
  // You should do some sanity checks on the timeout before using it. The
  // timeout come from the network, which cannot be trusted (unless you have
  // verified your client).
  std::optional<std::chrono::steady_clock::time_point> GetTimeout()
      const noexcept;

  // Returns compression algorithms acceptable by the client.
  //
  // Indexed by `flare::rpc::CompressionAlgorithm`.
  std::bitset<64> GetAcceptableCompressionAlgorithms() const noexcept;

  // Returns compression algorithm that is acceptable by the client and deems
  // best by Flare framework.
  rpc::CompressionAlgorithm GetPreferredCompressionAlgorithm() const noexcept;

  // Enable compression, using algorithm specified in argument.
  //
  // If `algorithm` is not acceptable by the client, the behavior is undefined.
  // @sa: `GetAcceptableCompressionAlgorithms`.
  //
  // If you don't have a preference about what algorithm to use, use what's
  // returned by `GetPreferredCompressionAlgorithm()`.
  void SetCompressionAlgorithm(rpc::CompressionAlgorithm algorithm);

  // Returns compression algorithm being used.
  rpc::CompressionAlgorithm GetCompressionAlgorithm() const noexcept;

  // No `SetTimeout` here. If you want to set a timeout for streaming RPC, use
  // `StreamReader/Writer::SetExpiration` instead.

  // For normal RPCs, certain protocols allow you to send an "attachment" along
  // with the message, which is more efficient compared to serializing the
  // attachment into the message.
  using RpcControllerCommon::GetRequestAttachment;
  using RpcControllerCommon::GetResponseAttachment;
  using RpcControllerCommon::SetResponseAttachment;

  // Get the request in its serialized form. This method is only available if
  // you've set `accept_request_raw_bytes` on the method being called (otherwise
  // the framework does not have a clue whether it shouldn't do deserialization
  // prior call to your implementation.)
  //
  // Note that it's your responsibility to check if the bytes is valid.
  using RpcControllerCommon::GetRequestRawBytes;

  // If set, bytes here is sent to the caller as the response. In this case,
  // whatever is filled into `response` passed to the method implementation is
  // silently dropped.
  //
  // It's your responsibility to make sure the bytes are valid binary
  // representation of the message being sent out.
  //
  // You should not use technique unless you have a solid reason to do so.
  using RpcControllerCommon::ClearResponseRawBytes;
  using RpcControllerCommon::GetResponseRawBytes;
  using RpcControllerCommon::SetResponseRawBytes;

  // In certain cases you might have pre-compressed bytes in hand, and want to
  // send them out as attachment. So as to avoid decompressing it first (in your
  // code) and compressing it again (by Flare), you can set this flag.
  //
  // It's your responsibility to make sure the bytes are compressed using the
  // same algorithm as of passed to `SetCompressionAlgorithm()`.
  void SetResponseAttachmentPrecompressed(bool compressed);

  // Returns whatever set by `SetResponseAttachmentPrecompressed`.
  bool GetResponseAttachmentPrecompressed() const noexcept;

  // For streaming RPCs, you can access the stream reader / writer here. Note
  // that these two methods may each only be called exactly once.
  using RpcControllerCommon::GetStreamReader;
  using RpcControllerCommon::GetStreamWriter;

  // If you'd prefer to `Future`-based interface, you can use these method for
  // streaming RPC.
  //
  // You shouldn't be using synchronous and asynchronous version at the same
  // time, as obvious.
  using RpcControllerCommon::GetAsyncStreamReader;
  using RpcControllerCommon::GetAsyncStreamWriter;

  // This allows you to know who is requesting you.
  using RpcControllerCommon::GetRemotePeer;

  // Time elapsed since we started serving the RPC.
  using RpcControllerCommon::GetElapsedTime;

  // Get timestamp of:
  //
  // - Syscall `read` returns the complete packet of this RPC.
  // - Fiber created for handling this RPC starts to run.
  // - The request has been parsed.
  //
  // CAUTION: Timestamp below can be non-monotical (w.r.t. `ReadSteadyClock()`)
  // under extreme conditions. If you only care about time elapsed during this
  // RPC, consider using `GetElapsedTime()`.
  std::chrono::steady_clock::time_point GetTimestampReceived() const noexcept;
  std::chrono::steady_clock::time_point GetTimestampDispatched() const noexcept;
  std::chrono::steady_clock::time_point GetTimestampParsed() const noexcept;

  // Add a log that will be reported to distributed tracing system. This
  // operation is only sensible if distributed tracing is enabled.
  //
  // Surely it's slow but I don't think the user is gonna adding too many logs
  // anyway.
  //
  // Note: OpenTracing supports providing a `key` with each log. However, given
  // that it's not widely supported (neither does TJG), we do not provide that
  // functionality for now. (Internally we always use a dummy string as `key`.)
  void AddTracingLog(std::string value);
  template <class T, class = decltype(std::to_string(std::declval<T>()))>
  void AddTracingLog(T&& value) {
    return AddTracingLog(std::to_string(value));
  }

  // Add a tag to the trace if there is one. This method is only sensible if
  // distributed tracing is enabled.
  //
  // It's suggested that you should always prefix your key with `user.` (or
  // something else such as your C++ namespace) to avoid name collisions with
  // the framework.
  void SetTracingTag(std::string key, std::string value);
  template <class T, class = decltype(std::to_string(std::declval<T>()))>
  void SetTracingTag(std::string key, T&& value) {
    return SetTracingTag(std::move(key), std::to_string(value));
  }

  // Tests if we're running in a dry-run environment.
  bool InDryRunEnvironment() const noexcept { return dry_run_env_; }

  // Tests if this request is sampled by binlog subsystem.
  bool IsCapturingBinlog() const noexcept {
    return is_capturing_binlog_.load(std::memory_order_relaxed);
  }

  // Get binlog correlation ID associated with this RPC. This is mostly used for
  // logging purpose.
  //
  // You don't need to pass it around when making outgoing RPCs.
  const std::string& GetBinlogCorrelationId() const noexcept {
    return binlog_correlation_id_;
  }

  // If you're using binlog facility, during "normal" run of your program, you
  // can set a context here for use in "dry run" mode.
  //
  // This method sets a context that will be passed back to the program when
  // doing a dry run (@sa: `GetBinlogTag()`).
  //
  // Usable in "normal" run only, CALLING THIS METHOD IN DRY RUN MODE RESULTS IN
  // UNDEFINED BEHAVIOR. (@sa: `InDryRunEnvironment()`)
  void SetBinlogTag(std::string key, std::string value);
  template <class T, class = decltype(std::to_string(std::declval<T>()))>
  void SetBinlogTag(std::string key, T&& value) {
    if (FLARE_UNLIKELY(IsCapturingBinlog())) {
      return SetBinlogTag(std::move(key), std::to_string(value));
    }
  }

  // Prevent this (sampled) RPC from dumping into binlog.
  void AbortBinlogCapture() noexcept;

  // Usable in dry-run mode. this method returns what was previously set by
  // `SetBinlogContext` in normal mode.
  //
  // CALLING THIS METHOD IN NON-DRY-RUN ENVIRONMENT RESULTS IN UNDEFINED
  // BEHAVIOR. (@sa: `InDryRunEnvironment()`)
  std::optional<std::string> GetBinlogTag(const std::string& key);
  template <class T>
  std::optional<T> GetBinlogTag(const std::string& key) {
    auto opt_str = GetBinlogTag(key);
    if (opt_str) {
      return TryParse<T>(*opt_str);
    }
    return std::nullopt;
  }

  // Reset this controller to its initial status.
  void Reset() override;

 private:
  FRIEND_TEST(RpcServerController, Basics);
  FRIEND_TEST(RpcServerController, Timeout);
  FRIEND_TEST(RpcServerController, Compression);

  friend class protobuf::Service;
  friend class protobuf::PassiveCallContextFactory;
  friend struct testing::detail::RpcControllerMaster;

  // Used by `protobuf::Service`.
  void SetTimeout(const std::chrono::steady_clock::time_point& timeout) {
    timeout_from_caller_ = timeout;
  }
  void SetAcceptableCompressionAlgorithm(std::uint64_t mask) {
    acceptable_comp_algos_ = mask;
  }
  void SetInDryRunEnvironment() noexcept { dry_run_env_ = true; }
  auto MutableUserTracingTags() noexcept { return &tracing_user_tags_; }
  auto MutableUserTracingLogs() noexcept { return &tracing_user_logs_; }
  auto MutableUserBinlogTagsForRead() noexcept {
    return &binlog_user_tags_for_read_;
  }
  const auto& GetUserBinlogTagsForWrite() const noexcept {
    return binlog_user_tags_for_write_;
  }
  void SetIsCapturingBinlog(bool f) noexcept {
    is_capturing_binlog_.store(f, std::memory_order_relaxed);
  }
  void SetBinlogCorrelationId(std::string id) noexcept {
    binlog_correlation_id_ = std::move(id);
  }

  // @sa: `WriteResponseImmediately`.
  void SetEarlyWriteResponseCallback(
      google::protobuf::Closure* callback) noexcept {
    early_write_resp_cb_ = callback;
  }
  google::protobuf::Closure* DestructiveGetEarlyWriteResponse() {
    return std::exchange(early_write_resp_cb_, nullptr);
  }

 private:
  // Not cared.
  void NotifyStreamProgress(const rpc::RpcMeta& meta) override {}
  void NotifyStreamCompletion(bool reached_eos) override {}

 private:
  int error_code_ = rpc::STATUS_SUCCESS;
  // If non-empty, provide the timeout set by the caller. The protocol we're
  // using must support this field for it to be useful.
  std::optional<std::chrono::steady_clock::time_point> timeout_from_caller_;
  bool dry_run_env_ = false;
  // No `tracing_sampled_`. If some backends failed during handle this RPC, we
  // might "force" the current span to be sampled.
  std::uint64_t acceptable_comp_algos_;
  rpc::CompressionAlgorithm comp_algo_ = rpc::COMPRESSION_ALGORITHM_NONE;
  google::protobuf::Closure* early_write_resp_cb_ = nullptr;
  std::string error_text_;
  bool resp_attachment_precompressed_ = false;

  std::mutex user_fields_lock_;
  std::vector<std::pair<std::string, std::string>> tracing_user_tags_;
  std::vector<std::pair<std::string, std::string>> tracing_user_logs_;
  // `std::unordered_map<std::string, std::string>` is unreasonably slow, to
  // boost performance in non-dry-run environment, we use a vector to hold the
  // tags temporarily, and only convert it to a map if we're really going to
  // dump this call.
  std::vector<std::pair<std::string, std::string>> binlog_user_tags_for_write_;
  std::atomic<bool> is_capturing_binlog_{};
  std::string binlog_correlation_id_;

  // Not protected as it's read only.
  std::unordered_map<std::string, std::string> binlog_user_tags_for_read_;
};

/////////////////////////////////////
// Implementation goes below.      //
/////////////////////////////////////

inline void RpcServerController::WriteResponseImmediately() {
  std::exchange(early_write_resp_cb_, nullptr)->Run();
}

inline std::optional<std::chrono::steady_clock::time_point>
RpcServerController::GetTimeout() const noexcept {
  return timeout_from_caller_;
}

std::chrono::steady_clock::time_point inline RpcServerController::
    GetTimestampReceived() const noexcept {
  return RpcControllerCommon::GetTimestamp(Timestamp::Received);
}

std::chrono::steady_clock::time_point inline RpcServerController::
    GetTimestampDispatched() const noexcept {
  return RpcControllerCommon::GetTimestamp(Timestamp::Dispatched);
}

std::chrono::steady_clock::time_point inline RpcServerController::
    GetTimestampParsed() const noexcept {
  return RpcControllerCommon::GetTimestamp(Timestamp::Parsed);
}

}  // namespace flare

#endif  // FLARE_RPC_PROTOCOL_PROTOBUF_RPC_SERVER_CONTROLLER_H_
