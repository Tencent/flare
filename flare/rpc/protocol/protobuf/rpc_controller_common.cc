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

#include "flare/rpc/protocol/protobuf/rpc_controller_common.h"

#include <chrono>

#include "flare/rpc/internal/stream_call_gate.h"
#include "flare/rpc/protocol/protobuf/message.h"

using namespace std::literals;

DEFINE_int32(flare_rpc_client_default_streaming_rpc_timeout_ms, 30000,
             "Default timeout for streaming RPCs. This option is applied to "
             "client-side streaming RPCs.");
DEFINE_int32(flare_rpc_server_default_streaming_rpc_timeout_ms, 30000,
             "Default timeout for streaming RPCs. This option is applied to "
             "server-side streaming RPCs.");

namespace flare::protobuf {

void RpcControllerCommon::CheckForStreamConsumption() {
  if (input_stream_) {
    FLARE_CHECK(
        input_stream_consumed_,
        "You must consume stream reader before destroying the controller.");
  }
  if (output_stream_) {
    FLARE_CHECK(
        output_stream_consumed_,
        "You must consume stream reader before destroying the controller.");
  }
}

void RpcControllerCommon::Reset() {
  CheckForStreamConsumption();

  // TODO(luobogao): Test if the controller is in-use.
  stream_timeout_ =
      ReadSteadyClock() +
      (server_side_
           ? FLAGS_flare_rpc_server_default_streaming_rpc_timeout_ms * 1ms
           : FLAGS_flare_rpc_client_default_streaming_rpc_timeout_ms * 1ms);
  streaming_call_ = false;
  use_eos_marker_ = true;
  memset(&tscs_, 0, sizeof(tscs_));
  tscs_[underlying_value(Timestamp::Start)] = ReadTsc();
  request_attachment_.Clear();
  response_attachment_.Clear();
  request_bytes_.reset();
  response_bytes_.reset();
  input_stream_ = std::nullopt;
  output_stream_ = std::nullopt;
  input_stream_consumed_ = output_stream_consumed_ = false;
}

void RpcControllerCommon::StartCancel() {
  // Cancellation is not implemented yet.
  FLARE_CHECK(!"Not supported.");
}

bool RpcControllerCommon::IsCanceled() const {
  // Cancellation is not implemented yet.
  FLARE_CHECK(!"Not supported.");
}

void RpcControllerCommon::NotifyOnCancel(google::protobuf::Closure* callback) {
  // Cancellation is not implemented yet.
  FLARE_CHECK(!"Not supported.");
}

void RpcControllerCommon::DisableEndOfStreamMarker() {
  use_eos_marker_ = false;
}

void RpcControllerCommon::SetStream(AsyncStreamReader<NativeMessagePtr> nis,
                                    AsyncStreamWriter<NativeMessagePtr> nos) {
  FLARE_CHECK(streaming_call_);
  nis.SetExpiration(GetStreamTimeout());
  nos.SetExpiration(GetStreamTimeout());
  input_stream_ = std::move(nis);
  output_stream_ = std::move(nos);
}

void RpcControllerCommon::SetStreamReader(
    AsyncStreamReader<NativeMessagePtr> reader) {
  FLARE_CHECK(streaming_call_);
  reader.SetExpiration(GetStreamTimeout());
  input_stream_ = std::move(reader);
}

void RpcControllerCommon::SetStreamWriter(
    AsyncStreamWriter<NativeMessagePtr> writer) {
  FLARE_CHECK(streaming_call_);
  writer.SetExpiration(GetStreamTimeout());
  output_stream_ = std::move(writer);
}

void RpcControllerCommon::SetStreamTimeout(
    std::chrono::steady_clock::time_point timeout) noexcept {
  stream_timeout_ = timeout;
  // For streaming RPCs, we need to update stream reader / writer's timeout as
  // well.
  if (input_stream_) {
    FLARE_CHECK(output_stream_);  // They are set in `SetStream` in the same
                                  // time.
    input_stream_->SetExpiration(stream_timeout_);
    output_stream_->SetExpiration(stream_timeout_);
  }
}

std::chrono::steady_clock::time_point RpcControllerCommon::GetStreamTimeout()
    const noexcept {
  return stream_timeout_;
}

void RpcControllerCommon::SetRpcMetaPrototype(const rpc::RpcMeta& meta) {
  meta_prototype_ = meta;
}

RpcControllerCommon::~RpcControllerCommon() { CheckForStreamConsumption(); }

}  // namespace flare::protobuf
