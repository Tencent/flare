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

#include "flare/rpc/protocol/protobuf/plugin/names.h"

#include <string>

#include "flare/base/string.h"

namespace flare::protobuf::plugin {

std::string ToNativeName(const std::string& s) { return Replace(s, ".", "::"); }

std::string GetInputType(const google::protobuf::MethodDescriptor* method) {
  return "::" + ToNativeName(method->input_type()->full_name());
}

std::string GetOutputType(const google::protobuf::MethodDescriptor* method) {
  return "::" + ToNativeName(method->output_type()->full_name());
}

std::string GetBasicServiceName(const google::protobuf::ServiceDescriptor* s) {
  return "Basic" + s->name();
}

std::string GetBasicStubName(const google::protobuf::ServiceDescriptor* s) {
  return s->name() + "_BasicStub";
}

std::string GetSyncServiceName(const google::protobuf::ServiceDescriptor* s) {
  return "Sync" + s->name();
}

std::string GetSyncStubName(const google::protobuf::ServiceDescriptor* s) {
  return s->name() + "_SyncStub";
}

std::string GetAsyncServiceName(const google::protobuf::ServiceDescriptor* s) {
  return "Async" + s->name();
}

std::string GetAsyncStubName(const google::protobuf::ServiceDescriptor* s) {
  return s->name() + "_AsyncStub";
}

std::string GetGdtCompatibleFutureServiceName(
    const google::protobuf::ServiceDescriptor* s) {
  return s->name() + "Future";
}

std::string GetGdtCompatibleFutureStubName(
    const google::protobuf::ServiceDescriptor* s) {
  return s->name() + "Future_Stub";
}

}  // namespace flare::protobuf::plugin
