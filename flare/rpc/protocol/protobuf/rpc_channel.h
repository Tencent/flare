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

#ifndef FLARE_RPC_PROTOCOL_PROTOBUF_RPC_CHANNEL_H_
#define FLARE_RPC_PROTOCOL_PROTOBUF_RPC_CHANNEL_H_

#ifndef FLARE_RPC_CHANNEL_SUPPRESS_INCLUDE_WARNING
#warning Use `flare/rpc/rpc_channel.h` instead.
#endif

#include <memory>
#include <string>

#include "gflags/gflags_declare.h"
#include "googletest/gtest/gtest_prod.h"
#include "protobuf/service.h"

#include "flare/base/internal/lazy_init.h"
#include "flare/base/ref_ptr.h"

DECLARE_int32(flare_rpc_channel_max_packet_size);

namespace flare {

class Endpoint;
class RpcClientController;
class Message;

namespace rpc::internal {

class StreamCallGate;
class StreamCallGateHandle;

}  // namespace rpc::internal

namespace protobuf::detail {

class MockChannel;

}

namespace protobuf {

struct ProtoMessage;

}

namespace tracing {

class QuickerSpan;

}  // namespace tracing

namespace binlog {

class OutgoingCallWriter;

}  // namespace binlog

// This one represents a virtual "channel" between us and the service. You may
// use this one to make RPCs to services that are using Protocol Buffers.
//
// In most cases, you should be using `Channel` via an `XxxService_Stub`
// instead of calling its methods yourself.
//
// TODO(luobogao): Given that `RpcChannel` should be made lightweight to create,
// we should support implicit channel creation like this:
//
//   // Create both a stub and (internally) a channel.
//   EchoService_SyncStub stub("flare://some-polaris-address");
class RpcChannel : public google::protobuf::RpcChannel {
 public:
  struct Options {
    // Maximum packet size.
    //
    // We won't allocate so many bytes immediately, neither won't we keep the
    // buffer such large after it has been consumed. This is only an upper limit
    // to keep you safe in case of a malfunctioning server.
    std::size_t maximum_packet_size = FLAGS_flare_rpc_channel_max_packet_size;

    // If non-empty, NSLB specified here will be used in place of the default
    // NSLB mechanism of protocol being used.
    std::string override_nslb;
  };

  RpcChannel();
  ~RpcChannel();

  // Almost the same as constructing a channel and calls `Open` on it, except
  // that failure in opening channel won't raise an error. Instead, any RPCs
  // made through the resulting channel will fail with `STATUS_INVALID_CHANNEL`.
  RpcChannel(std::string address,
             const Options& options = internal::LazyInitConstant<Options>());

  // `address` uses URI syntax (@sa: RFC 3986). NSLB is inferred from `scheme`
  // being used in `address. In general, polaris is the perferred NSLB.
  //
  // Making RPCs on a channel that haven't been `Open`ed successfully result in
  // undefined behavior.
  bool Open(std::string address,
            const Options& options = internal::LazyInitConstant<Options>());

  // For internal use. Do NOT call this method.
  //
  // Must be called before entering multi-threaded environment.
  static void RegisterMockChannel(protobuf::detail::MockChannel* channel);

 protected:
  // `request` / `response` is ignored if the `method` accepts a stream of
  // requests (i.e., streaming RPC is used.). In this case requests should be
  // read / written via `GetStreamReader()` / `GetStreamWriter()` of
  // `controller`.
  //
  // If raw bytes are used, `request` / `response` might be `nullptr`, so don't
  // use reference here.
  //
  // For non-streaming call, if `done` is not provided, it's a blocking call.
  // `done` must be `nullptr` for streaming calls.
  void CallMethod(const google::protobuf::MethodDescriptor* method,
                  google::protobuf::RpcController* controller,
                  const google::protobuf::Message* request,
                  google::protobuf::Message* response,
                  google::protobuf::Closure* done) override;

 private:
  struct RpcCompletionDesc;

  FRIEND_TEST(Channel, L5);
  void CallMethodWritingBinlog(const google::protobuf::MethodDescriptor* method,
                               RpcClientController* controller,
                               const google::protobuf::Message* request,
                               google::protobuf::Message* response,
                               google::protobuf::Closure* done);

  void CallMethodWithRetry(const google::protobuf::MethodDescriptor* method,
                           RpcClientController* controller,
                           const google::protobuf::Message* request,
                           google::protobuf::Message* response,
                           google::protobuf::Closure* done,
                           std::size_t retries_left);

  // Make an RPC.
  //
  // This method is carefully designed so that you can call it concurrently even
  // with the same controller. This is essential for implementing things such as
  // backup request.
  //
  // The caller is responsible for ensuring that `response` is not shared with
  // anyone else.
  //
  // `cb` is called with `(const RpcCompletionDesc&)`.
  template <class F>
  void CallMethodNoRetry(const google::protobuf::MethodDescriptor* method,
                         const google::protobuf::Message* request,
                         const RpcClientController& controller,
                         google::protobuf::Message* response, F&& cb);

  void CallStreamingMethod(
      const google::protobuf::MethodDescriptor* method,
      const google::protobuf::Message* request,
      // There is no point in providing `response` here. Unless it's a
      // client-streaming-only RPC, `response` makes no sense. However, if it's
      // a client-streaming RPC, only the user knows when the requests are all
      // sent. After (the user) sending out all requests, we can hardly help the
      // user in waiting for response, which is better handled by `StreamReader`
      // (even if there's only one response message.).
      RpcClientController* controller, google::protobuf::Closure* done);

  std::uint32_t NextCorrelationId() const noexcept;

  template <class F>
  bool GetPeerOrFailEarlyForFastCall(
      const google::protobuf::MethodDescriptor& method, Endpoint* peer,
      std::uintptr_t* nslb_ctx, F&& cb);

  void CreateNativeRequestForFastCall(
      const google::protobuf::MethodDescriptor& method,
      const google::protobuf::Message* request,
      const RpcClientController& controller, protobuf::ProtoMessage* to);

  tracing::QuickerSpan StartTracingSpanFor(
      const Endpoint& peer, const google::protobuf::MethodDescriptor* method);

  void FinishTracingSpanWith(int completion_status, tracing::QuickerSpan* span,
                             bool forcibly_sampled);

  binlog::OutgoingCallWriter* StartDumpingFor(
      const google::protobuf::MethodDescriptor* method,
      RpcClientController* ctlr);

  void FinishDumpingWith(binlog::OutgoingCallWriter* logger,
                         RpcClientController* ctlr);

  std::string GetBinlogCorrelationId(
      const google::protobuf::MethodDescriptor* method,
      const RpcClientController& ctlr);

  void CopyInterestedFieldsFromMessageToController(
      const RpcCompletionDesc& completion_desc, RpcClientController* ctlr);

 private:
  rpc::internal::StreamCallGateHandle GetFastCallGate(const Endpoint& ep);
  rpc::internal::StreamCallGateHandle GetStreamCallGate(const Endpoint& ep);
  RefPtr<rpc::internal::StreamCallGate> CreateCallGate(const Endpoint& ep);

 private:
  // To avoid bring in too many headers, we define our members inside `Impl`.
  struct Impl;

  Options options_;
  std::string address_;
  std::unique_ptr<Impl> impl_;
};

}  // namespace flare

#endif  // FLARE_RPC_PROTOCOL_PROTOBUF_RPC_CHANNEL_H_
