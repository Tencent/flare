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

#ifndef FLARE_RPC_INTERNAL_STREAM_CALL_GATE_H_
#define FLARE_RPC_INTERNAL_STREAM_CALL_GATE_H_

#include <chrono>
#include <limits>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>

#include "gflags/gflags.h"
#include "gtest/gtest_prod.h"

#include "flare/base/align.h"
#include "flare/base/delayed_init.h"
#include "flare/base/function.h"
#include "flare/base/maybe_owning.h"
#include "flare/base/net/endpoint.h"
#include "flare/base/object_pool.h"
#include "flare/base/ref_ptr.h"
#include "flare/fiber/execution_context.h"
#include "flare/fiber/mutex.h"
#include "flare/fiber/work_queue.h"
#include "flare/io/event_loop.h"
#include "flare/io/native/stream_connection.h"
#include "flare/rpc/binlog/log_writer.h"
#include "flare/rpc/internal/correlation_id.h"
#include "flare/rpc/internal/correlation_map.h"
#include "flare/rpc/internal/stream.h"
#include "flare/rpc/internal/stream_io_adaptor.h"
#include "flare/rpc/protocol/stream_protocol.h"

DECLARE_int32(flare_rpc_client_stream_concurrency);

namespace flare::rpc::internal {

// A "call gate" owns a connection, i.e., no load balance / fault tolerance /
// name resolving will be done here. Use `XxxChannel` instead if that's what
// you want.
//
// This is generally used by `XxxChannel` (via `StreamCallGatePool`).
//
// Thread-safe.
class StreamCallGate : private StreamConnectionHandler,
                       public RefCounted<StreamCallGate> {
 public:
  using MessagePtr = std::unique_ptr<Message>;

  // Timestamps. Not applicable to streaming RPC.
  struct Timestamps {
    std::uint64_t sent_tsc;
    std::uint64_t received_tsc;
    std::uint64_t parsed_tsc;
  };

  // Final status of an RPC.
  enum class CompletionStatus { Success, IoError, ParseError, Timeout };

  // Arguments for making a fast call.
  struct FastCallArgs {
    // `msg_on_success` is applicable only if `status` is `Success`.
    Function<void(CompletionStatus status, MessagePtr msg_on_success,
                  const Timestamps& timestamps)>
        completion;

    // Execution context `completion` should be run in. If `nullptr` is given,
    // completion is run in fiber with no execution context.
    RefPtr<fiber::ExecutionContext> exec_ctx;

    // Passed to protocol object, opaque to us.
    Controller* controller;
  };

  struct Options {
    // TlsContext tls_context;
    MaybeOwning<StreamProtocol> protocol;
    std::size_t maximum_packet_size = 0;
  };

  StreamCallGate();
  ~StreamCallGate();

  // On failure, the call gate is set to "unhealthy" state. Healthy state can be
  // checked via `Healthy()`.
  //
  // We don't return failure from this method to simplify implementation of our
  // caller. Handling failure of gate open can be hard. Besides, failure should
  // be rare, most of the failures are a result of exhaustion of ephemeral
  // ports.
  void Open(const Endpoint& address, Options options);

  // Check if the call gate is healthy (i.e, it's still connected, not in an
  // error state.).
  bool Healthy() const;

  // Manually mark this call gate as unhealthy.
  void SetUnhealthy();

  // All pending RPCs are immediately completed with `kGateClosing`.
  void Stop();
  void Join();

  // Some basics about the call gate.
  const Endpoint& GetEndpoint() const;
  const StreamProtocol& GetProtocol() const;

  // Fast path for simple RPCs (one request / one response).
  //
  // 64-bit correlation ID is NOT supported. AFAICT we don't generate 64-bit
  // correlation ID in our system.
  void FastCall(const Message& m, PooledPtr<FastCallArgs> args,
                std::chrono::steady_clock::time_point timeout);

  // Cancel a previous call to `FastCall`.
  //
  // Returns `nullptr` is the call has already been completed (e.g., by
  // receiving its response from network).
  PooledPtr<FastCallArgs> CancelFastCall(std::uint32_t correlation_id);

  // For RPCs that involves multiple requests / multiple responses.
  //
  // Note that timeout is not support for streaming RPCs, it's hard to define
  // what does "timeout" mean in streaming case. Use `SetExpiration` on streams
  // returned instead.
  std::pair<AsyncStreamReader<MessagePtr>, AsyncStreamWriter<MessagePtr>>
  StreamCall(std::uint64_t correlation_id, Controller* controller);

 public:
  // Get event loop this gate is attached to.
  //
  // FOR INTERNAL USE ONLY.
  EventLoop* GetEventLoop();

 private:
  FRIEND_TEST(StreamCallGatePoolTest, RemoveBrokenGate);

  struct alignas(hardware_destructive_interference_size) FastCallContext {
    fiber::Mutex lock;

    std::uint64_t correlation_id;
    std::uint64_t timeout_timer = 0;
    Timestamps timestamps;
    PooledPtr<FastCallArgs> user_args;
  };

  friend struct PoolTraits<FastCallContext>;

  struct StreamContext {
    bool closed = false;    // Set by `OnStreamClose`.
    bool eos_seen = false;  // For detecting double end-of-stream marker.
    std::uint64_t correlation_id;
    Controller* controller;
    std::unique_ptr<rpc::detail::StreamIoAdaptor> adaptor;
  };

  // Called in dedicated fiber. Blocking is OK.
  void ServiceFastCallCompletion(std::unique_ptr<Message> msg,
                                 PooledPtr<FastCallContext> ctx,
                                 std::uint64_t tsc);

  void OnAttach(StreamConnection*) override;  // Not cared.
  void OnDetach() override;                   // Not cared.
  void OnWriteBufferEmpty() override;         // Not cared.

  // Called upon data is written out.
  //
  // `ctx` is correlation id if it's associated with a stream, 0 otherwise.
  void OnDataWritten(std::uintptr_t ctx) override;

  // Called upon new data is available.
  DataConsumptionStatus OnDataArrival(NoncontiguousBuffer* buffer) override;

  // Called upon remote close.  All outstanding RPCs are completed with error.
  void OnClose() override;

  // Called upon error. All outstanding RPCs are completed with error.
  void OnError() override;

  // Initialize underlying connection.
  bool InitializeConnection(const Endpoint& ep);

  // Write data out. This method also reconnect the underlying socket if it's
  // closed.
  bool WriteOut(NoncontiguousBuffer buffer, std::uintptr_t ctx);

  // Allocate a context associated with `correlation_id`. `f` is called to
  // initialize the context.
  //
  // Initialization of the new call context is sequenced before its insertion
  // into the map.
  template <class F>
  void AllocateRpcContextFastCall(std::uint32_t correlation_id, F&& init);

  // Reclaim rpc context if `correlation_id` is associated with a fast call,
  // otherwise `nullptr` is returned.
  //
  // Note that this method also returns `nullptr` if `correlation_id` does not
  // exist at all. This may somewhat degrade performance of processing
  // streams, we might optimize it some day later.
  PooledPtr<FastCallContext> TryReclaimRpcContextFastCall(
      std::uint32_t correlation_id);

  // Traverse in-use fast-call correlations.
  //
  // `f` is called with *RPC correlation ID* (not including connection
  // correlation ID) and call context.
  template <class F>
  void ForEachRpcContextFastCall(F&& f);

  // Allocate context for stream calls.
  template <class F>
  void AllocateRpcContextStream(std::uint64_t correlation_id, F&& init);

  // Prevent further call to `LockRpcContextStreamIfPresent` from noticing this
  // stream context.
  void DisableRpcContextStream(std::uint64_t correlation_id);

  // If `correlation_id` present, `f` is called with rpc-context-map's lock
  // held. `f` is not called at all if `correlation_id` does not exist.
  //
  // If `f` returns a non-void type `T`, `std::optional<T>` is returned,
  // otherwise `bool` is returned. Return value evaluates to `true` if `f` is
  // called.
  //
  // `f` accepts `StreamContext&`.
  //
  // Do NOT block or call user's code in `f`.
  //
  // The behavior is undefined if `correlation_id` is associated with fast call.
  template <
      class F, class FR = std::invoke_result_t<F, StreamContext*>,
      class R = std::conditional_t<std::is_void_v<FR>, bool, std::optional<FR>>>
  R LockRpcContextStreamIfPresent(std::uint64_t correlation_id, F&& f);

  // Reclaim rpc context associated with `correlation_id`.
  //
  // The behavior is undefined if `correlation_id` is associated with fast call.
  template <class F>
  void ReclaimRpcContextStream(std::uint64_t correlation_id, F&& cb);

  // Serialize `message`.
  NoncontiguousBuffer WriteMessage(const Message& message,
                                   Controller* controller) const;

  // Raise an error if the corresponding RPC is found.
  static void RaiseErrorIfPresentFastCall(
      CorrelationMap<PooledPtr<FastCallContext>>* map,
      std::uint32_t conn_correlation_id, std::uint32_t rpc_correlation_id,
      CompletionStatus status);
  void RaiseErrorIfPresentStreamCall(std::uint64_t correlation_id,
                                     CompletionStatus status);

  // Complete all on-going RPCs with failure status.
  void UnsafeRaiseErrorGlobally();

  // Called after both input stream and output stream closed.
  void OnStreamClosed(std::uint64_t correlation_id);

  // Called when all pending callbacks to the stream have completed.
  void OnStreamCleanup(std::uint64_t correlation_id);

 private:
  Options options_{};
  Endpoint endpoint_;
  EventLoop* event_loop_ = nullptr;
  RefPtr<NativeStreamConnection> conn_;
  std::atomic<bool> healthy_{true};

  // Connection correlation ID. Fast-calls need this to access correlation map.
  std::uint32_t conn_correlation_id_{NewConnectionCorrelationId()};
  CorrelationMap<PooledPtr<FastCallContext>>* correlation_map_;

  // FIXME: Stream calls are slow.
  //
  // Do NOT use `std::mutex` here, we'll be calling code that trigger fiber
  // scheduler under the lock. (`std::mutex` does not support unlocking in
  // different pthread worker.)
  fiber::Mutex stream_ctxs_lock_;
  std::unordered_map<std::uint64_t, std::unique_ptr<StreamContext>>
      stream_ctxs_;  // `PooledPtr`?

  // We don't need a stream reaper for each connection. Streaming RPCs are rare.
  // It's initialized on first call to `StreamCall`.
  std::once_flag stream_reaper_init_;
  DelayedInit<fiber::WorkQueue> stream_reaper_;
};

}  // namespace flare::rpc::internal

namespace flare {

template <>
struct PoolTraits<rpc::internal::StreamCallGate::FastCallArgs> {
  static constexpr auto kType = PoolType::MemoryNodeShared;
  static constexpr auto kLowWaterMark = 8192;
  static constexpr auto kHighWaterMark =
      std::numeric_limits<std::size_t>::max();
  static constexpr auto kMaxIdle = std::chrono::seconds(10);
  static constexpr auto kMinimumThreadCacheSize = 1024;
  static constexpr auto kTransferBatchSize = 1024;

  static void OnPut(rpc::internal::StreamCallGate::FastCallArgs* ptr) {
    FLARE_CHECK(!ptr->completion,
                "Call context is destroyed without calling user's completion "
                "callback.");
    ptr->exec_ctx = nullptr;
  }
};

}  // namespace flare

#endif  // FLARE_RPC_INTERNAL_STREAM_CALL_GATE_H_
