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

#ifndef FLARE_RPC_PROTOCOL_PROTOBUF_RPC_CLIENT_CONTROLLER_H_
#define FLARE_RPC_PROTOCOL_PROTOBUF_RPC_CLIENT_CONTROLLER_H_

#ifndef FLARE_RPC_CLIENT_CONTROLLER_SUPPRESS_INCLUDE_WARNING
#warning Use `flare/rpc/rpc_client_controller.h` instead.
#endif

#include <string>

#include "gflags/gflags_declare.h"
#include "gtest/gtest_prod.h"

#include "flare/base/status.h"
#include "flare/rpc/internal/stream_call_gate_pool.h"
#include "flare/rpc/protocol/protobuf/call_context.h"
#include "flare/rpc/protocol/protobuf/rpc_controller_common.h"

DECLARE_int32(flare_rpc_client_default_rpc_timeout_ms);

namespace flare {

namespace testing::detail {

class MockRpcChannel;

}  // namespace testing::detail

namespace protobuf {

class RpcChannelForDryRun;

}

// This controller controls a single RPC. It's used on client-side.
class RpcClientController : public protobuf::RpcControllerCommon {
 public:
  RpcClientController() : protobuf::RpcControllerCommon(false) {}

  // Test if the call failed.
  //
  // It makes no sense to call this method before RPC is completed.
  bool Failed() const override;

  // Returns error code of this call, or `STATUS_SUCCESS` if no failure
  // occurred.
  int ErrorCode() const override;  // @sa: `Status` in `rpc_meta.proto`.

  // Returns whatever the server used to describe the error.
  std::string ErrorText() const override;

  // Set timeout for this RPC.
  //
  // If not set, the default timeout (2s for fast calls, 30s for streaming
  // calls.) is applied.
  //
  // Both time point (of whatever clock type) and duration are accepted.
  void SetTimeout(internal::SteadyClockView timeout) noexcept;
  std::chrono::steady_clock::time_point GetTimeout() const noexcept;

  // Make sure that your call is idempotent before enabling this.
  //
  // If not set, 1 is the default (i.e., no retry).
  //
  // Note that this method has no effect on streaming RPC.
  void SetMaxRetries(std::size_t max_retries);
  std::size_t GetMaxRetries() const;

  // If set, the response is NOT parsed by the framework. In this case whatever
  // response message you used with service stub is not touched (you can even
  // use a `nullptr` for it). You can get the response in its serialized form by
  // calling `GetResponseRawBytes()`.
  //
  // Note that this method has no effect on streaming RPC.
  void SetAcceptResponseRawBytes(bool f /* = true? */) noexcept;
  bool GetAcceptResponseRawBytes() const noexcept;

  // For normal RPCs, certain protocols allow you to send an "attachment" along
  // with the message, which is more efficient compared to serializing the
  // attachment into the message.
  using RpcControllerCommon::GetRequestAttachment;
  using RpcControllerCommon::GetResponseAttachment;
  using RpcControllerCommon::SetRequestAttachment;

  // If `SetAcceptResponseRawBytes` was set when issuing this RPC, you can get
  // the response in its serialized form from this method.
  //
  // Note that it's your responsibility to check if the raw bytes are indeed the
  // expected message.
  using RpcControllerCommon::GetResponseRawBytes;

  // If you somehow already have serialized request at hand, you can use this
  // method to pass it to the framework. In this case, whatever is passed to
  // service stub as `request` is ignored (you can even use `nullptr` in this
  // case).
  //
  // It's your responsibility to make sure the bytes are valid binary
  // representation of the message being sent out.
  //
  // This is an "advanced" technique, and does not fit neatly with the
  // framework. Do not use it, unless you have a strong reason to do so.
  using RpcControllerCommon::ClearRequestRawBytes;
  using RpcControllerCommon::GetRequestRawBytes;
  using RpcControllerCommon::SetRequestRawBytes;

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

  // Time elapsed since the RPC started.
  using RpcControllerCommon::GetElapsedTime;

  // Get timestamp of:
  //
  // - Timestamp prior call to syscall `writev`.
  // - Syscall `read` returns a complete packet of the response.
  // - The response has been parsed.
  //
  // Timestamps below are not applicable for streaming calls. Neither are they
  // defined should the (fast) call failed.
  //
  // CAUTION: Timestamp below can be non-monotical (w.r.t. `ReadSteadyClock()`)
  // under extreme conditions. If you only care about time elapsed during this
  // RPC, consider using `GetElapsedTime()`.
  std::chrono::steady_clock::time_point GetTimestampSent();
  std::chrono::steady_clock::time_point GetTimestampReceived();
  std::chrono::steady_clock::time_point GetTimestampParsed();

  // Message body used compression algorithm.
  void SetCompressionAlgorithm(
      rpc::CompressionAlgorithm compression_algorithm) {
    compression_algorithm_ = compression_algorithm;
  }
  rpc::CompressionAlgorithm GetCompressionAlgorithm() const noexcept {
    return compression_algorithm_;
  }

  // You can set a correlation ID for each call you made. When doing a dry-run,
  // this ID is used for matching requests and responses.
  //
  // Note that you don't have to set this explicitly unless you're calling the
  // same backend more than once when processing an incoming RPC. If all
  // backends you call is represented by different URI, the framework can match
  // requests and responses all on itself.
  //
  // You don't have to prepend binlog correlation ID of the incoming RPC you're
  // handling, the framework will do this for you.
  void SetBinlogCorrelationId(std::string id) noexcept {
    binlog_correlation_id_ = std::move(id);
  }

  // FIXME: Is calling `AddTracingLog()` on `RpcClientController` making sense?

  // Reset this controller to its initial status.
  //
  // If you reuse the controller for successive RPCs, you must call `Reset()` in
  // between. We cannot do this for you, as it's only after your `done` has been
  // called, the controller can be `Reset()`-ed. But after your `done` is
  // called, it's possible that the controller itself is destroyed (presumably
  // by you), and by that time, to avoid use-after-free, we cannot `Reset()` the
  // controller.
  void Reset() override;

 private:  // We need package visibility here.
  FRIEND_TEST(RpcClientController, Basics);
  friend class RpcChannel;  // It needs access to several private methods below.
  friend class protobuf::RpcChannelForDryRun;
  friend class testing::detail::MockRpcChannel;
  friend struct testing::detail::RpcControllerMaster;

  // Make sure the controller is NOT in used and mark it as being used.
  void PrecheckForNewRpc();

  // Get timeout in relative (to `last_reset_`) time.
  std::chrono::nanoseconds GetRelativeTimeout() const noexcept {
    return timeout_ - last_reset_;
  }

  // Context for streaming RPCs. It's initialized lazily.
  struct StreamingRpcContext {
    protobuf::ProactiveCallContext call_ctx;
    rpc::internal::StreamCallGateHandle call_gate;
  };

  void InitializeStreamingRpcContext();
  StreamingRpcContext* GetStreamingRpcContext();

  // `done` is called upon RPC completion.
  void SetCompletion(google::protobuf::Closure* done);

  // Correlation ID used by binlog.
  const std::string& GetBinlogCorrelationId() const noexcept {
    return binlog_correlation_id_;
  }

  // Notifies RPC completion. This method is only used for fast calls.
  //
  // If no response is received, the overload with manually-provided status is
  // called.
  void NotifyCompletion(Status status);

  // Called for each received message.
  void NotifyStreamProgress(const rpc::RpcMeta& meta) override;

  // Called when the streaming call finished.
  void NotifyStreamCompletion(bool reached_eos) override;

  // Not used.
  void SetFailed(const std::string& reason) override;

 private:
  bool in_use_ = false;
  bool completed_ = false;

  // User settings.
  std::size_t max_retries_ = 1;
  std::chrono::steady_clock::time_point last_reset_{ReadSteadyClock()};
  std::chrono::steady_clock::time_point timeout_{
      last_reset_ +
      std::chrono::milliseconds(FLAGS_flare_rpc_client_default_rpc_timeout_ms)};
  bool accept_resp_in_bytes_ = false;
  rpc::CompressionAlgorithm compression_algorithm_ =
      rpc::COMPRESSION_ALGORITHM_NONE;
  google::protobuf::Closure* completion_ = nullptr;

  // RPC State.
  std::optional<Status> rpc_status_;
  std::string binlog_correlation_id_;

  std::unique_ptr<StreamingRpcContext> streaming_rpc_ctx_;
};

}  // namespace flare

#endif  // FLARE_RPC_PROTOCOL_PROTOBUF_RPC_CLIENT_CONTROLLER_H_
