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

#include "flare/rpc/protocol/protobuf/rpc_options.h"

#include <optional>

#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"

#include "flare/base/logging.h"
#include "flare/rpc/rpc_options.pb.h"

namespace flare::protobuf {

namespace detail {

// Below are options defined by poppy.

// @sa: `common/rpc/xxx_rpc_service.pb.h`.
template <class T, auto kType, bool kPacked = false>
using ServiceOption = google::protobuf::internal::ExtensionIdentifier<
    google::protobuf::ServiceOptions,
    google::protobuf::internal::PrimitiveTypeTraits<T>, kType, kPacked>;
template <class T, auto kType, bool kPacked = false>
using MethodOption = google::protobuf::internal::ExtensionIdentifier<
    google::protobuf::MethodOptions,
    google::protobuf::internal::PrimitiveTypeTraits<T>, kType, kPacked>;

ServiceOption<google::protobuf::int32, 5> wechat_protocol_magic(10022, 0);
ServiceOption<google::protobuf::int32, 5> qzone_protocol_version(10000, 0);
ServiceOption<bool, 8> qzone_protocol_response_checksum(10001, false);

MethodOption<google::protobuf::int32, 5> wechat_protocol_cmd(10011, 0);
MethodOption<google::protobuf::int32, 5> qzone_protocol_cmd(10000, 0);
MethodOption<bool, 8> streaming_response(10003, false);

}  // namespace detail

std::optional<int> TryGetQZoneServiceId(
    const google::protobuf::ServiceDescriptor* service) {
  auto&& opts = service->options();
  if (opts.HasExtension(detail::qzone_protocol_version)) {
    return opts.GetExtension(detail::qzone_protocol_version);
  } else if (opts.HasExtension(qzone_service_id)) {
    return opts.GetExtension(qzone_service_id);
  }
  return std::nullopt;
}

std::optional<int> TryGetQZoneMethodId(
    const google::protobuf::MethodDescriptor* method) {
  auto&& opts = method->options();
  if (opts.HasExtension(detail::qzone_protocol_cmd)) {
    return opts.GetExtension(detail::qzone_protocol_cmd);
  } else if (opts.HasExtension(qzone_method_id)) {
    return opts.GetExtension(qzone_method_id);
  }
  return std::nullopt;
}

bool IsClientStreamingMethod(const google::protobuf::MethodDescriptor* method) {
  return method->client_streaming();
}

bool IsServerStreamingMethod(const google::protobuf::MethodDescriptor* method) {
  if (method->options().HasExtension(detail::streaming_response) &&
      method->options().GetExtension(detail::streaming_response) !=
          method->server_streaming() &&
      !method->options().GetExtension(
          testing_only_no_warning_on_gdt_streaming_response)) {
    google::protobuf::SourceLocation location;
    if (!method->GetSourceLocation(&location)) {
      location.start_line = 0;
    }
    FLARE_LOG_ERROR_ONCE(
        "{}:{}: Option `gdt.streaming_response` is no longer supported. You "
        "should use `stream` keyword instead.",
        method->file()->name(), location.start_line);
  }
  return method->server_streaming() ||
         // Well we actually still support it ... for some time.
         method->options().GetExtension(detail::streaming_response);
}

bool IsStreamingMethod(const google::protobuf::MethodDescriptor* method) {
  return IsClientStreamingMethod(method) || IsServerStreamingMethod(method);
}

}  // namespace flare::protobuf
