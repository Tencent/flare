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

#ifndef FLARE_RPC_PROTOCOL_PROTOBUF_RPC_CONTROLLER_COMMON_H_
#define FLARE_RPC_PROTOCOL_PROTOBUF_RPC_CONTROLLER_COMMON_H_

#include <chrono>
#include <memory>

#include "gflags/gflags_declare.h"
#include "google/protobuf/service.h"

#include "flare/base/buffer.h"
#include "flare/base/internal/time_view.h"
#include "flare/base/net/endpoint.h"
#include "flare/base/tsc.h"
#include "flare/rpc/internal/stream.h"
#include "flare/rpc/protocol/protobuf/message.h"
#include "flare/rpc/protocol/protobuf/rpc_meta.pb.h"

// I don't see much point in using different flag for client-side and
// server-side. Yet I kept them separate for consistency with non-streaming RPCs
// (there only client-side timeout is applicable).
DECLARE_int32(flare_rpc_client_default_streaming_rpc_timeout_ms);
DECLARE_int32(flare_rpc_server_default_streaming_rpc_timeout_ms);

namespace flare::testing::detail {

struct RpcControllerMaster;
class MockRpcChannel;

}  // namespace flare::testing::detail

namespace flare::protobuf {

template <class T>
class TypedInputStreamProvider;
template <class T>
class TypedOutputStreamProvider;

// Implementation detail.
//
// Depending on whether you're writing server-side or client-side code, you
// should use `RpcServerController` or `RpcClientController` instead.
class RpcControllerCommon : public google::protobuf::RpcController {
  // Methods are selectively exposed by subclasses. Refer to them for
  // documentation.
 protected:
  enum class Timestamp {
    Start,
    Dispatched,  // Not applicable at client side.
    Sent,        // Not applicable at server side.
    Received,
    Parsed,

    Count  // Not a timestamp.
  };

  explicit RpcControllerCommon(bool server_side) : server_side_(server_side) {
    Reset();
  }
  ~RpcControllerCommon();

  // If there's an attachment attached to the request / response, it's stored
  // here.
  //
  // Not all protocol supports this. If you're using attachment on a protocol
  // not supporting attachment, either `STATUS_NOT_SUPPORTED` is returned
  // (preferrably), or a warning is written to log.

  void SetRequestAttachment(NoncontiguousBuffer attachment) noexcept {
    request_attachment_ = std::move(attachment);
  }
  const NoncontiguousBuffer& GetRequestAttachment() const noexcept {
    return request_attachment_;
  }
  void SetResponseAttachment(NoncontiguousBuffer attachment) noexcept {
    response_attachment_ = std::move(attachment);
  }
  const NoncontiguousBuffer& GetResponseAttachment() const noexcept {
    return response_attachment_;
  }

  // In certain cases (e.g., for perf. reasons.), it may be desired that request
  // / response should not be parsed until the last minute (if ever). In this
  // case, the message is stored here in its serialized format. In case we're
  // the one who should serialize the message, the already-serialized buffer is
  // used instead of the corresponding message (which is silently ignored).
  //
  // This is a somewhat "advanced" technique, and does not fit neatly with the
  // framework (raw bytes never do). You shouldn't only use this technique
  // unless you have a strong reason to do so.
  void SetRequestRawBytes(NoncontiguousBuffer buffer) noexcept {
    request_bytes_ = std::move(buffer);
  }
  const NoncontiguousBuffer& GetRequestRawBytes() const noexcept {
    FLARE_CHECK(!!request_bytes_, "Request was not filled as opaque bytes.");
    return *request_bytes_;
  }
  bool HasRequestRawBytes() const noexcept { return !!request_bytes_; }
  void ClearRequestRawBytes() noexcept { request_bytes_ = std::nullopt; }
  // Response..
  void SetResponseRawBytes(NoncontiguousBuffer buffer) noexcept {
    response_bytes_ = std::move(buffer);
  }
  const NoncontiguousBuffer& GetResponseRawBytes() const noexcept {
    FLARE_CHECK(!!response_bytes_, "Response was not filled as opaque bytes.");
    return *response_bytes_;
  }
  bool HasResponseRawBytes() const noexcept { return !!response_bytes_; }
  void ClearResponseRawBytes() noexcept { response_bytes_ = std::nullopt; }

  // Both time point (of whatever clock type) and duration are accepted.
  //
  // TODO(luobogao): I'd like to remove these two methods. For client side, the
  // timeout timer can be set in `RpcChannel`, for server-side, it can be set
  // prior to calling user's code (and, possibly later, by user's code itself).
  void SetStreamTimeout(
      const std::chrono::steady_clock::time_point& timeout) noexcept;
  std::chrono::steady_clock::time_point GetStreamTimeout() const noexcept;

  void SetIsStreaming() noexcept { streaming_call_ = true; }
  bool IsStreaming() const noexcept { return streaming_call_; }

  // Test if this is a server-side controller.
  bool IsServerSideController() const noexcept { return server_side_; }

  // Check if the corresponding stream is still alive.
  bool IsStreamReaderUntouched() const noexcept {
    return input_stream_ && !input_stream_consumed_;
  }
  bool IsStreamWriterUntouched() const noexcept {
    return output_stream_ && !output_stream_consumed_;
  }

  // Crash the program if the stream(s) is not consumed by the user.
  void CheckForStreamConsumption();

  // Get an output stream for receiving responses.
  //
  // This method is only available if the method (RPC) you're calling returns a
  // stream of responses.
  //
  // The stream returned must be closed before the controller is destroyed.
  //
  // If there's pending call on the stream returned, `RpcControllerCommon` may
  // not be used.
  template <class T>
  StreamReader<T> GetStreamReader();

  // Get an input stream for streaming requests.
  //
  // This method is only available if the method (RPC) you're calling receives a
  // stream of requests.
  //
  // The stream returned must be closed before the controller is destroyed.
  //
  // If there's pending call on the stream returned, `RpcControllerCommon` may
  // not be used.
  template <class T>
  StreamWriter<T> GetStreamWriter();

  // Asynchronous version of `GetStreamReader()` / `GetStreamWriter()`.
  //
  // Note that you can't be using both synchronous & asynchronous version at the
  // same time.
  template <class T>
  AsyncStreamReader<T> GetAsyncStreamReader();
  template <class T>
  AsyncStreamWriter<T> GetAsyncStreamWriter();

  // Reset the controller.
  void Reset() override;

  // Cancellation is not implemented yet.
  void StartCancel() override;
  bool IsCanceled() const override;
  void NotifyOnCancel(google::protobuf::Closure* callback) override;

  // Get error code of this RPC, or `STATUS_SUCCESS` if no failure occurred.
  virtual int ErrorCode() const = 0;

  // const Endpoint& GetLocalPeer() const;

  // Disable end-of-stream marker, the underlying protocol does not support it.
  void DisableEndOfStreamMarker();

  // Set I/O stream for this call. This method is only called for streaming
  // RPCs.
  using NativeMessagePtr = std::unique_ptr<Message>;
  void SetStream(AsyncStreamReader<NativeMessagePtr> nis,
                 AsyncStreamWriter<NativeMessagePtr> nos);

  // In case you only want to provide a reader or a writer (but not both),
  // methods below can be used instead.
  void SetStreamReader(AsyncStreamReader<NativeMessagePtr> reader);
  void SetStreamWriter(AsyncStreamWriter<NativeMessagePtr> writer);

  // Used by streaming RPCs (for responses).
  //
  // Except for `eos`, every fields in `meta` is copied into each on-wire packet
  // we send.
  void SetRpcMetaPrototype(const rpc::RpcMeta& meta);

  // Set local & remote peer address.
  void SetRemotePeer(const Endpoint& remote_peer) noexcept {
    remote_peer_ = std::move(remote_peer);
  }
  // void SetLocalPeer(Endpoint local_peer);

  // Get remote peer's address.
  //
  // Server side only.
  const Endpoint& GetRemotePeer() const noexcept { return remote_peer_; }

  // Get elapsed time since request. It's called in client side in most cases.
  // Maybe used in server side to finish request in advance.
  std::chrono::nanoseconds GetElapsedTime() const noexcept {
    return DurationFromTsc(tscs_[underlying_value(Timestamp::Start)],
                           ReadTsc());
  }

  // Set timestamp of `ts`. FOR INTERNAL USE ONLY.
  //
  // To access individual timestamps, use `GetXxxTimestamp` provided by
  // `RpcXxxController`.
  void SetTimestamp(Timestamp ts, std::uint64_t tsc = ReadTsc()) noexcept {
    std::uint32_t x = underlying_value(ts);
    FLARE_CHECK_LT(x, underlying_value(Timestamp::Count));
    tscs_[x] = tsc;
  }
  std::chrono::steady_clock::time_point GetTimestamp(
      Timestamp ts) const noexcept {
    std::uint32_t x = underlying_value(ts);
    FLARE_CHECK_LT(x, underlying_value(Timestamp::Count));
    return TimestampFromTsc(tscs_[x]);
  }

  // For internal use.

  // Simulated by streaming rpc with no payload (but with an attachment).
  //
  // We're responsible for cutting the stream if its too large.
  //
  // AsyncStreamReader<Buffer> OfferByteStream();
  // AsyncStreamWriter<Buffer> AcceptByteStream();

  // For streaming RPC, this callback is called for each received message.
  virtual void NotifyStreamProgress(const rpc::RpcMeta& meta) = 0;

  // For streaming RPC, this callback is called when the call finished.
  virtual void NotifyStreamCompletion(bool reached_eos) = 0;

 private:
  friend class testing::detail::MockRpcChannel;
  friend struct testing::detail::RpcControllerMaster;
  friend class RpcChannel;

  template <class F>
  friend class protobuf::TypedInputStreamProvider;
  template <class F>
  friend class protobuf::TypedOutputStreamProvider;

  // Create a stream provider. Used by `GetXxxStream()`.
  template <class T>
  RefPtr<StreamWriterProvider<T>> GetStreamWriterProvider();
  template <class T>
  RefPtr<StreamReaderProvider<T>> GetStreamReaderProvider();

 private:
  bool server_side_;  // Never changes.

  std::chrono::steady_clock::time_point stream_timeout_;
  bool streaming_call_;
  bool use_eos_marker_;
  Endpoint remote_peer_;

  std::uint64_t tscs_[underlying_value(Timestamp::Count)];

  // If there was an attachment attached to the request / response, it's saved
  // here.
  NoncontiguousBuffer request_attachment_;
  NoncontiguousBuffer response_attachment_;

  // If present, they are / should be used instead of the corresponding message.
  std::optional<NoncontiguousBuffer> request_bytes_;
  std::optional<NoncontiguousBuffer> response_bytes_;

  // Streaming RPCs.
  rpc::RpcMeta meta_prototype_;  // Prototype for generating `Message`s.
  bool input_stream_consumed_ = false;
  bool output_stream_consumed_ = false;
  std::optional<AsyncStreamReader<NativeMessagePtr>> input_stream_;
  std::optional<AsyncStreamWriter<NativeMessagePtr>> output_stream_;
};

// This class translate `T` to `ProtoMessage` which is recognized by the
// framework.
template <class T>
class TypedOutputStreamProvider : public StreamWriterProvider<T> {
 public:
  using MessagePtr = std::unique_ptr<Message>;

  explicit TypedOutputStreamProvider(RpcControllerCommon* ctlr_);
  ~TypedOutputStreamProvider();

  void SetExpiration(std::chrono::steady_clock::time_point expires_at) override;
  void Write(T object, bool last, Function<void(bool)> cb) override;
  void Close(Function<void()> cb) override;

 private:
  std::unique_ptr<Message> TranslateMessage(T object, bool eos);
  void WriteEosMarker(Function<void()> cb);

 private:
  RpcControllerCommon* ctlr_;
  bool last_sent_{false};
  bool first_sent_{false};
};

// This class translate `ProtoMessage`, which is use by the framework, to `T`
// for end-user's use.
template <class T>
class TypedInputStreamProvider : public StreamReaderProvider<T> {
 public:
  explicit TypedInputStreamProvider(RpcControllerCommon* ctlr);
  ~TypedInputStreamProvider();

  void SetExpiration(std::chrono::steady_clock::time_point expires_at) override;
  void Peek(Function<void(Expected<T, StreamError>*)> cb) override;
  void Read(Function<void(Expected<T, StreamError>)> cb) override;
  void Close(Function<void()> cb) override;

 private:
  using MessagePtr = RpcControllerCommon::NativeMessagePtr;

  // Returns `true` if `msg` is a valid `T`, and in the meantime, it carries an
  // end-of-stream marker.
  bool InlineEndOfStreamMarkerPresentInEmptyPayload(
      const Expected<MessagePtr, StreamError>& msg);

  Expected<T, StreamError> TranslateMessage(
      Expected<MessagePtr, StreamError>&& msg);

  void TranslateAndCall(Expected<MessagePtr, StreamError>&& msg,
                        Function<void(Expected<T, StreamError>)>&& cb);

  void NotifyControllerProgress(const Expected<MessagePtr, StreamError>& e);
  void NotifyControllerCompletion(bool success);

 private:
  bool closed_ = false;
  bool completion_notified_ = false;
  bool seen_inline_eos_ = false;
  RpcControllerCommon* ctlr_;
};

template <class T>
StreamReader<T> RpcControllerCommon::GetStreamReader() {
  FLARE_CHECK(!input_stream_consumed_,
              "`GetStreamReader()` may only be called once.");
  input_stream_consumed_ = true;
  return StreamReader<T>(GetStreamReaderProvider<T>());
}

template <class T>
StreamWriter<T> RpcControllerCommon::GetStreamWriter() {
  FLARE_CHECK(!output_stream_consumed_,
              "`GetStreamWriter()` may only be called once.");
  output_stream_consumed_ = true;
  return StreamWriter<T>(GetStreamWriterProvider<T>());
}

template <class T>
AsyncStreamReader<T> RpcControllerCommon::GetAsyncStreamReader() {
  FLARE_CHECK(!input_stream_consumed_,
              "`GetAsyncStreamReader()` may only be called once.");
  input_stream_consumed_ = true;
  return AsyncStreamReader<T>(GetStreamReaderProvider<T>());
}

template <class T>
AsyncStreamWriter<T> RpcControllerCommon::GetAsyncStreamWriter() {
  FLARE_CHECK(!output_stream_consumed_,
              "`GetAsyncStreamWriter()` may only be called once.");
  output_stream_consumed_ = true;
  return AsyncStreamWriter<T>(GetStreamWriterProvider<T>());
}

template <class T>
RefPtr<StreamWriterProvider<T>> RpcControllerCommon::GetStreamWriterProvider() {
  FLARE_CHECK(!!output_stream_,
              "No stream is associated with this controller.");
  return MakeRefCounted<protobuf::TypedOutputStreamProvider<T>>(this);
}

template <class T>
RefPtr<StreamReaderProvider<T>> RpcControllerCommon::GetStreamReaderProvider() {
  FLARE_CHECK(!!input_stream_, "No stream is associated with this controller.");
  return MakeRefCounted<protobuf::TypedInputStreamProvider<T>>(this);
}

}  // namespace flare::protobuf

#include "flare/rpc/protocol/protobuf/rpc_controller_common_inl.h"

#endif  // FLARE_RPC_PROTOCOL_PROTOBUF_RPC_CONTROLLER_COMMON_H_
