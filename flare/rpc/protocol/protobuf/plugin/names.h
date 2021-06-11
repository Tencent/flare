// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef FLARE_RPC_PROTOCOL_PROTOBUF_PLUGIN_NAMES_H_
#define FLARE_RPC_PROTOCOL_PROTOBUF_PLUGIN_NAMES_H_

#include <string>

#include "protobuf/descriptor.h"

#include "flare/rpc/protocol/protobuf/plugin/code_writer.h"

namespace flare::protobuf::plugin {

// Several predefined insertion points.
constexpr auto kInsertionPointIncludes = "includes";
constexpr auto kInsertionPointNamespaceScope = "namespace_scope";
constexpr auto kInsertionPointGlobalScope = "global_scope";

// Convert Protocol Buffer's fully qualified name to cpp style.
std::string ToNativeName(const std::string& s);

// Get fully qualified type name of method's input type.
std::string GetInputType(const google::protobuf::MethodDescriptor* method);

// Get fully qualified type name of method's output type.
std::string GetOutputType(const google::protobuf::MethodDescriptor* method);

// Mangles service / stub's name.

// Basic service is exactly what `cc_generic_service = true` would generates.
std::string GetBasicServiceName(const google::protobuf::ServiceDescriptor* s);
std::string GetBasicStubName(const google::protobuf::ServiceDescriptor* s);

// Async service / stub?

// Sync service is not inherently performs bad in flare since we're using fiber
// anyway.
//
// This stub also provides a better interface for calling streaming methods.
std::string GetSyncServiceName(const google::protobuf::ServiceDescriptor* s);
std::string GetSyncStubName(const google::protobuf::ServiceDescriptor* s);

// `Future`-based interfaces. They can be handy when calling multiple backends
// simultaneously.
std::string GetAsyncServiceName(const google::protobuf::ServiceDescriptor* s);
std::string GetAsyncStubName(const google::protobuf::ServiceDescriptor* s);

// This one is API compatible with our old plugin (`gdt_future_rpc`).
std::string GetGdtCompatibleFutureServiceName(
    const google::protobuf::ServiceDescriptor* s);
std::string GetGdtCompatibleFutureStubName(
    const google::protobuf::ServiceDescriptor* s);

}  // namespace flare::protobuf::plugin

#endif  // FLARE_RPC_PROTOCOL_PROTOBUF_PLUGIN_NAMES_H_
