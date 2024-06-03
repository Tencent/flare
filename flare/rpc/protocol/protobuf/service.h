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

#ifndef FLARE_RPC_PROTOCOL_PROTOBUF_SERVICE_H_
#define FLARE_RPC_PROTOCOL_PROTOBUF_SERVICE_H_

#include <atomic>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "google/protobuf/service.h"
#include "jsoncpp/value.h"

#include "flare/base/deferred.h"
#include "flare/base/internal/hash_map.h"
#include "flare/base/internal/test_prod.h"
#include "flare/base/maybe_owning.h"
#include "flare/rpc/protocol/stream_service.h"

namespace flare {

class RpcServerController;

}  // namespace flare

namespace flare::protobuf {

struct ProtoMessage;

// This class acts as an adaptor from `google::protobuf::Service` to
// `flare::StreamService`.
//
// This class is also responsible for registering factories for messages used by
// its `impl`'s methods (via `MethodDescriber::Register`).
class Service : public StreamService {  // Only `StreamService` is supported
                                        // (for now).
 public:
  ~Service();

  void AddService(MaybeOwning<google::protobuf::Service> impl);

  const experimental::Uuid& GetUuid() const noexcept override;

  bool Inspect(const Message& message, const Controller& controller,
               InspectionResult* result) override;

  bool ExtractCall(const std::string& serialized_ctx,
                   const std::vector<std::string>& serialized_pkt_ctxs,
                   ExtractedCall* extracted) override;

  ProcessingStatus FastCall(
      std::unique_ptr<Message>* request,
      const FunctionView<std::size_t(const Message&)>& writer,
      Context* context) override;

  ProcessingStatus StreamCall(
      AsyncStreamReader<std::unique_ptr<Message>>* input_stream,
      AsyncStreamWriter<std::unique_ptr<Message>>* output_stream,
      Context* context) override;

  void Stop() override;
  void Join() override;

 private:
  void WriteFastCallBinlog(const ProtoMessage& req, const ProtoMessage& resp);
  void CaptureFastCallDryRunResult(const ProtoMessage& req,
                                   const ProtoMessage& resp);

 private:
  FLARE_FRIEND_TEST(ServiceFastCallTest, RejectedDelayedFastCall);
  FLARE_FRIEND_TEST(ServiceFastCallTest, AcceptedDelayedFastCall);
  FLARE_FRIEND_TEST(ServiceFastCallTest, RejectedMaxOngoingFastCall);
  FLARE_FRIEND_TEST(ServiceFastCallTest, AcceptedMaxOngoingFastCall);
  FLARE_FRIEND_TEST(ServiceFastCallTest, MaxOngoingFlag);

  struct alignas(64) AlignedInt {
    std::atomic<int> value{};
  };

  struct MethodDesc {
    google::protobuf::Service* service;
    const google::protobuf::MethodDescriptor* method;
    const google::protobuf::Message* request_prototype;  // For doing dry-run.
    const google::protobuf::Message* response_prototype;
    bool is_streaming;
    std::chrono::nanoseconds max_queueing_delay{
        std::chrono::nanoseconds::max()};
    std::uint32_t max_ongoing_requests;

    // Applicable only `max_ongoing_request` is not 0.
    std::unique_ptr<AlignedInt> ongoing_requests;
  };

  // Returns [nullptr, nullptr] if the request is rejected.
  const MethodDesc* SanityCheckOrRejectEarlyForFastCall(
      const Message& msg,
      const FunctionView<std::size_t(const Message&)>& resp_writer,
      const Context& ctx) const;

  void InitializeServerControllerForFastCall(const ProtoMessage& msg,
                                             const Context& ctx,
                                             RpcServerController* ctlr);

  // Call user's service implementation. The response is written out by this
  // method, *and* saved into `resp_msg`.
  //
  // The reason why we don't delay response writeout until this method's
  // completes is for better responsiveness.
  void InvokeUserMethodForFastCall(
      const MethodDesc& method, const ProtoMessage& req_msg,
      ProtoMessage* resp_msg, RpcServerController* ctlr,
      const FunctionView<std::size_t(const Message&)>& writer, Context* ctx);

  void CompleteTracingPostOperationForFastCall(RpcServerController* ctlr,
                                               Context* ctx);
  void CompleteBinlogPostOperationForFastCall(const ProtoMessage& req,
                                              const ProtoMessage& resp,
                                              const RpcServerController& ctlr,
                                              Context* ctx);

  // Determines if we have the resource to process the requested method. An
  // empty object is returned if the request should be rejected.
  Deferred AcquireProcessingQuotaOrReject(const ProtoMessage& msg,
                                          const MethodDesc& method,
                                          const Context& ctx);

  void CreateNativeResponse(const MethodDesc& method_desc,
                            const ProtoMessage& request,
                            std::unique_ptr<google::protobuf::Message> resp_ptr,
                            RpcServerController* ctlr, ProtoMessage* response);
  const MethodDesc* FindHandler(const std::string& method_name) const;

 private:
  std::vector<MaybeOwning<google::protobuf::Service>> services_;

  // Used for detecting "Service not found" error. This is only used on
  // error-path.
  //
  // Elements here references string of `ServiceDescriptor`.
  std::unordered_set<std::string_view> registered_services_;

  // This is to workaround a common misuse: Users tend to free their service
  // class before destroying `flare::Server` (and the `protobuf::Service` here).
  // By the time we're destroyed, objects referenced by `services_` may have
  // already gone. Fortunately, even if user's service class has gone, their
  // descriptors are long-lived. Therefore, we make a copy here for
  // unregistration..
  std::vector<const google::protobuf::ServiceDescriptor*> service_descs_;

  // Keyed by `MethodDescriptor::full_name()`.
  internal::HashMap<std::string, MethodDesc> method_descs_;
};

}  // namespace flare::protobuf

#endif  // FLARE_RPC_PROTOCOL_PROTOBUF_SERVICE_H_
