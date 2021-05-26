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

#ifndef FLARE_RPC_PROTOCOL_STREAM_SERVICE_H_
#define FLARE_RPC_PROTOCOL_STREAM_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "flare/base/experimental/uuid.h"
#include "flare/base/function_view.h"
#include "flare/base/net/endpoint.h"
#include "flare/rpc/internal/stream.h"
#include "flare/rpc/protocol/controller.h"
#include "flare/rpc/protocol/message.h"

namespace flare {

class StreamProtocol;

// The implementation is responsible for processing messages received by
// `Server`. Only messages extracted by `StreamProtocol` is tried on
// `StreamService`.
//
// Note that the current design inevitably incurs some performance penalty if
// there are too many `StreamService` registered with `Server`, as `Server` need
// to do a linear search for each `Message` to find a `StreamService` to handle
// it. But in all cases I've seen in our codebase, there are just a few (1 ~ 3
// in most cases) of them. So don't worry.
//
// If (very unlikely) we indeed found this to be a performance bottleneck, we
// could add a `MessageType()` tag to `Message`, ask `StreamProtocol`s to fill
// it, add a `AcceptableMessageTypes()` to `StreamService`, and use a hash map
// to find a match for each message. This way the linear search is avoided.
class StreamService {
 public:
  virtual ~StreamService() = default;

  // This structure is used for passing data between the framework and the
  // implementation.

  struct Context {
    ///////////////////////////////////////////////////////
    // Fields below are read-only to the implementation. //
    ///////////////////////////////////////////////////////

    // Timestamps below are taken from TSC. If you need timestamps in
    // `std::chrono::` format, convert them yourself (@sa: `tsc.h`).
    //
    // Not applicable to streaming RPC.
    std::uint64_t received_tsc;    // Packet received from syscall.
    std::uint64_t dispatched_tsc;  // Fiber dedicated to this RPC starts to run.
    std::uint64_t parsed_tsc;      // The request is fully parsed.

    // Size of the incoming packet. Not applicable to streaming RPC.
    std::size_t incoming_packet_size = 0;

    // Address of local & remote side.
    Endpoint local_peer;
    Endpoint remote_peer;

    // `Controller` object created by protocol object. It's provided here in
    // case you need it.
    Controller* controller;

    // TODO(luobogao): Use `StreamProtocolCharacteristics` instead once it's
    // refactored out.
    bool streaming_call_no_eos_marker = false;

    ////////////////////////////////////////////////////////////////////
    // Fields below should be filled by the implementation on return. //
    ////////////////////////////////////////////////////////////////////

    // Status code of this RPC.
    int status;  // Not using `flare::Status` as that one does not get along
                 // well with HTTP.

    // If set, and tracing is enabled, this RPC is forcibly reported.
    bool advise_trace_forcibly_sampled = false;

    // FIXME: Perhaps we should accept tracing tags / logs here and move it into
    // tracing span ourselves?

    // FIXME: Ditto for binlog context.

    ////////////////////////////////////////
    // Fields below are in/out parameter. //
    ////////////////////////////////////////

    // Nothing yet.
  };

  // The framework may need several information about the method being called,
  // it should be returned by `Inspect`.
  struct InspectionResult {
    // Fully qualified name of method being called.
    std::string_view method;
  };

  // Extracted by `ExtractCall`.
  struct ExtractedCall {
    // All messages that were serialized into `Context::serialized_binlog`.
    std::vector<std::unique_ptr<Message>> messages;

    // `Controller` that was serialized into `Context::serialized_binlog`.
    std::unique_ptr<Controller> controller;
  };

  enum class ProcessingStatus {
    // Everything worked as intended. The request will be freed by the
    // framework. If any reply should be made, it is already sent by the
    // implementation.
    //
    // For fast calls, the framework may hold some state until `on_completion`
    // is called.
    Processed,

    // This status should be returned if the underlying connection should be
    // closed ASAP (after finishing sending all pending buffers). This is useful
    // for short connections.
    Completed,

    // Completion callback (if applicable) is not expected to be called in the
    // following cases:

    // We're overloaded. Reject this request.
    Overloaded,

    // The request is dropped, and no response should be sent.
    Dropped,

    // This status indicates the message is recognized, but it not processed as
    // it's (likely) corrupted. The framework will close the connection if this
    // value is returned.
    Corrupted,

    // For whatever reasons, the message is not expected. (e.g., a stream
    // message is forwarded to a service that does not support stream call at
    // all.)
    Unexpected
  };

  // Get UUID of the implementation.
  //
  // Returning duplicate from different implementation leads to undefined
  // behavior.
  //
  // NOTICE: If there turns out to be several classes that declare this method,
  // we can use a dedicated `Identifiable` interface instead.
  virtual const experimental::Uuid& GetUuid() const noexcept = 0;

  // Inspects `message` and if the implementation recognizes the message,
  // returns some basic information needed by the framework. The framework
  // (caller) is responsible for making sure `result` is not dirty, the
  // implementation does not necessarily reset `result` before filling it.
  //
  // Make it fast.
  //
  // Note: We're not using `std::optional<InspectionResult>` (as return value
  // type) here for perf. reasons. `std::optional<T>` really burdens optimizer,
  // esp. when `T` is not trivial.
  virtual bool Inspect(const Message& message, const Controller& controller,
                       InspectionResult* result) = 0;

  // Extracts `Message`(s) and (optionally) `Controller` serialized by `XxxCall`
  // below into `Context`.
  //
  // This method is only called when doing dry-run. You can always returns
  // failure here if you're not going to support it.
  //
  // Returns `false` on failure, the serialized call is dropped in this case.
  virtual bool ExtractCall(const std::string& serialized_ctx,
                           const std::vector<std::string>& serialized_pkt_ctxs,
                           ExtractedCall* extracted) = 0;

  // For both `FastCall` and `StreamCall`:
  //
  // These two methods are responsible for dealing with facilities such as
  // binlog / tracing / ... . They should check the individual `xxxx_context`
  // and take appropriate actions to help the framework to finish its job.

  // Handles RPCs in one-response-to-one-request fashion.
  //
  // Called outside of event loop's workers. Blocking is acceptable.
  //
  // To be more responsiveness, `writer` is provided for the implementation to
  // write response (before even returning from this method). The Implementation
  // should call `writer` exactly once (on success).
  //
  // `request` should be left untouched if a failure status is returned.
  virtual ProcessingStatus FastCall(
      std::unique_ptr<Message>* request,
      const FunctionView<std::size_t(const Message&)>& writer,
      Context* context) = 0;

  // Handles a stream from the requesting client.
  //
  // Both stream should be closed by the implementation on success. On failure
  // they should be left untouched.
  virtual ProcessingStatus StreamCall(
      AsyncStreamReader<std::unique_ptr<Message>>* input_stream,
      AsyncStreamWriter<std::unique_ptr<Message>>* output_stream,
      Context* context) = 0;

  virtual void Stop() = 0;
  virtual void Join() = 0;
};

}  // namespace flare

#endif  // FLARE_RPC_PROTOCOL_STREAM_SERVICE_H_
