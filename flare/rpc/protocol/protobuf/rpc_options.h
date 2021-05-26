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

#ifndef FLARE_RPC_PROTOCOL_PROTOBUF_RPC_OPTIONS_H_
#define FLARE_RPC_PROTOCOL_PROTOBUF_RPC_OPTIONS_H_

#include <optional>

#include "thirdparty/protobuf/service.h"

// To be compatible with Poppy's RPC options (`common/rpc/rpc_options.proto`),
// we need to either:
//
// - Have a dependency on that Protocol Buffers target, or
// - Do some trick here.
//
// This file (, along with `flare/rpc/rpc_options.proto`) implements the second
// approach.

namespace flare::protobuf {

// Surprisingly enough, there are indeed some services use 0 as either service ID
// or method ID in their definitions.
//
// Both options defined by Poppy and us are recognized.
std::optional<int> TryGetQZoneServiceId(
    const google::protobuf::ServiceDescriptor* service);
std::optional<int> TryGetSvrkitServiceId(
    const google::protobuf::ServiceDescriptor* service);
std::optional<int> TryGetQZoneMethodId(
    const google::protobuf::MethodDescriptor* method);
std::optional<int> TryGetSvrkitMethodId(
    const google::protobuf::MethodDescriptor* method);

// FIXME: Remove method below, there's little sense in keeping support
// `gdt.stream_response` given that `stream` keyword is officially supported.

// Test if the given method support streaming RPC.
bool IsClientStreamingMethod(const google::protobuf::MethodDescriptor* method);
bool IsServerStreamingMethod(const google::protobuf::MethodDescriptor* method);
// Returns true if either of above holds.
bool IsStreamingMethod(const google::protobuf::MethodDescriptor* method);

}  // namespace flare::protobuf

#endif  // FLARE_RPC_PROTOCOL_PROTOBUF_RPC_OPTIONS_H_
