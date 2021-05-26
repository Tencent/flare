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

#include "flare/rpc/protocol/protobuf/plugin/basic_decl_generator.h"

#include <string>
#include <vector>

#include "thirdparty/protobuf/compiler/cpp/cpp_helpers.h"

#include "flare/base/string.h"
#include "flare/rpc/protocol/protobuf/plugin/names.h"
#include "flare/rpc/protocol/protobuf/rpc_options.h"

using namespace std::literals;

namespace flare::protobuf::plugin {

void BasicDeclGenerator::GenerateService(
    const google::protobuf::FileDescriptor* file,
    const google::protobuf::ServiceDescriptor* service, CodeWriter* writer) {
  // Generate service class' definition in header.
  std::vector<std::string> method_decls;
  for (int i = 0; i != service->method_count(); ++i) {
    auto&& method = service->method(i);
    auto s = Format(
        "virtual void {method}(\n"
        "    ::google::protobuf::RpcController* controller,\n"
        "    const {input_type}* request,\n"
        "    {output_type}* response,\n"
        "    ::google::protobuf::Closure* done);",
        fmt::arg("method", method->name()),
        fmt::arg("input_type", GetInputType(method)),
        fmt::arg("output_type", GetOutputType(method)));
    method_decls.push_back(s);
  }
  *writer->NewInsertionToHeader(kInsertionPointNamespaceScope) = Format(
      "class {stub};\n"
      "\n"
      "class {service} : public ::google::protobuf::Service {{\n"
      " protected:\n"
      "  {service}() = default;\n"
      "\n"
      " public:\n"
      "  using Stub = {stub};\n"
      "  virtual ~{service}() = default;\n"
      "\n"
      "  {methods}\n"
      "\n"
      "  static const ::google::protobuf::ServiceDescriptor* descriptor();\n"
      "  const ::google::protobuf::ServiceDescriptor* GetDescriptor();\n"
      "\n"
      "  void CallMethod(const ::google::protobuf::MethodDescriptor* method,\n"
      "                  ::google::protobuf::RpcController* controller,\n"
      "                  const ::google::protobuf::Message* request,\n"
      "                  ::google::protobuf::Message* response,\n"
      "                  ::google::protobuf::Closure* done);\n"
      "\n"
      "  const ::google::protobuf::Message& GetRequestPrototype(\n"
      "      const ::google::protobuf::MethodDescriptor* method) const;\n"
      "  const ::google::protobuf::Message& GetResponsePrototype(\n"
      "      const ::google::protobuf::MethodDescriptor* method) const;\n"
      "\n"
      " private:\n"
      "  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS({service});\n"
      "}};\n"
      "\n",
      fmt::arg("stub", GetBasicStubName(service)),
      fmt::arg("service", GetBasicServiceName(service)),
      fmt::arg("methods", Replace(Join(method_decls, "\n"), "\n", "\n  ")));

  // Generate service's implementation.
  std::vector<std::string> method_impls;
  std::vector<std::string> call_method_impls;
  std::vector<std::string> get_request_prototype_impls;
  std::vector<std::string> get_response_prototype_impls;
  for (int i = 0; i != service->method_count(); ++i) {
    auto&& method = service->method(i);

    method_impls.emplace_back() = Format(
        "void {service}::{method}(\n"
        "    ::google::protobuf::RpcController* controller,\n"
        "    const {input_type}*,\n"
        "    {output_type}*,\n"
        "    ::google::protobuf::Closure* done) {{\n"
        "  controller->SetFailed(\"Method {method}() not implemented.\");\n"
        "  done->Run();\n"
        "}}",
        fmt::arg("service", GetBasicServiceName(service)),
        fmt::arg("method", method->name()),
        fmt::arg("input_type", GetInputType(method)),
        fmt::arg("output_type", GetOutputType(method)));
    call_method_impls.emplace_back() = Format(
        "case {index}:\n"
        "  {method}(\n"
        "      controller,\n"
        "      ::google::protobuf::down_cast<const {input_type}*>(request),\n"
        "      ::google::protobuf::down_cast<{output_type}*>(response),\n"
        "      done);\n"
        "  break;",
        fmt::arg("index", method->index()), fmt::arg("method", method->name()),
        fmt::arg("input_type", GetInputType(method)),
        fmt::arg("output_type", GetOutputType(method)));
    get_request_prototype_impls.emplace_back() = Format(
        "case {index}:\n"
        "  return {input_type}::default_instance();",
        fmt::arg("index", method->index()),
        fmt::arg("input_type", GetInputType(method)));
    get_response_prototype_impls.emplace_back() = Format(
        "case {index}:\n"
        "  return {output_type}::default_instance();",
        fmt::arg("index", method->index()),
        fmt::arg("output_type", GetOutputType(method)));
  }
  *writer->NewInsertionToSource(kInsertionPointNamespaceScope) += Format(
      "{methods}\n"
      "\n"
      "const ::google::protobuf::ServiceDescriptor*\n"
      "{service}::descriptor() {{\n"
      "  return flare_rpc::GetServiceDescriptor({svc_idx});\n"
      "}}\n"
      "\n"
      "const ::google::protobuf::ServiceDescriptor*\n"
      "{service}::GetDescriptor() {{\n"
      "  return flare_rpc::GetServiceDescriptor({svc_idx});\n"
      "}}\n"
      "\n"
      "void {service}::CallMethod(\n"
      "    const ::google::protobuf::MethodDescriptor* method,\n"
      "    ::google::protobuf::RpcController* controller,\n"
      "    const ::google::protobuf::Message* request,\n"
      "    ::google::protobuf::Message* response,\n"
      "    ::google::protobuf::Closure* done) {{\n"
      "  GOOGLE_DCHECK_EQ(method->service(),\n"
      "                   flare_rpc::GetServiceDescriptor({svc_idx}));\n"
      "  switch (method->index()) {{\n"
      "    {call_method_cases}\n"
      "  default:\n"
      "    GOOGLE_LOG(FATAL) <<\n"
      "        \"Bad method index; this should never happen.\";\n"
      "  }}\n"
      "}}\n"
      "\n"
      "const ::google::protobuf::Message& {service}::GetRequestPrototype(\n"
      "    const ::google::protobuf::MethodDescriptor* method) const {{\n"
      "  GOOGLE_DCHECK_EQ(method->service(),\n"
      "                   flare_rpc::GetServiceDescriptor({svc_idx}));\n"
      "  switch (method->index()) {{\n"
      "    {get_request_prototype_cases}\n"
      "  default:\n"
      "    GOOGLE_LOG(FATAL) <<\n"
      "        \"Bad method index; this should never happen.\";\n"
      "    return *::google::protobuf::MessageFactory::generated_factory()\n"
      "        ->GetPrototype(method->input_type());\n"
      "  }}\n"
      "}}\n"
      "\n"
      "const ::google::protobuf::Message& {service}::GetResponsePrototype(\n"
      "    const ::google::protobuf::MethodDescriptor* method) const {{\n"
      "  GOOGLE_DCHECK_EQ(method->service(),\n"
      "                   flare_rpc::GetServiceDescriptor({svc_idx}));\n"
      "  switch (method->index()) {{\n"
      "    {get_response_prototype_cases}\n"
      "  default:\n"
      "    GOOGLE_LOG(FATAL) <<\n"
      "        \"Bad method index; this should never happen.\";\n"
      "    return *::google::protobuf::MessageFactory::generated_factory()\n"
      "        ->GetPrototype(method->output_type());\n"
      "  }}\n"
      "}}\n"
      "\n",
      fmt::arg("file", file->name()),
      fmt::arg("service", GetBasicServiceName(service)),
      fmt::arg("file_ns", google::protobuf::compiler::cpp::FileLevelNamespace(
                              file->name())),
      fmt::arg("svc_idx", service->index()),
      fmt::arg("methods", Join(method_impls, "\n")),
      fmt::arg("call_method_cases",
               Replace(Join(call_method_impls, "\n"), "\n", "\n    ")),
      fmt::arg(
          "get_request_prototype_cases",
          Replace(Join(get_request_prototype_impls, "\n"), "\n", "\n    ")),
      fmt::arg(
          "get_response_prototype_cases",
          Replace(Join(get_response_prototype_impls, "\n"), "\n", "\n    ")));
}

void BasicDeclGenerator::GenerateStub(
    const google::protobuf::FileDescriptor* file,
    const google::protobuf::ServiceDescriptor* service, CodeWriter* writer) {
  // Code in header.
  std::vector<std::string> method_decls;
  for (int i = 0; i != service->method_count(); ++i) {
    auto&& method = service->method(i);
    method_decls.emplace_back() = Format(
        "void {method}(\n"
        "    ::google::protobuf::RpcController* controller,\n"
        "    const {input_type}* request,\n"
        "    {output_type}* response,\n"
        "    ::google::protobuf::Closure* done);",
        fmt::arg("method", method->name()),
        fmt::arg("input_type", GetInputType(method)),
        fmt::arg("output_type", GetOutputType(method)));
  }
  // Method `channel()` is not const method in protobuf's generated code,
  // although I see no reason in not making it const.
  *writer->NewInsertionToHeader(kInsertionPointNamespaceScope) = Format(
      "class {stub} : public {service} {{\n"
      " public:\n"
      "  {stub}(::google::protobuf::RpcChannel* channel);\n"
      "  {stub}(\n"
      "      ::google::protobuf::RpcChannel* channel,\n"
      "      ::google::protobuf::Service::ChannelOwnership ownership);\n"
      "  ~{stub}();\n"
      "\n"
      "  {methods}\n"
      "\n"
      "  ::google::protobuf::RpcChannel* channel() {{ return channel_; }}\n"
      "\n"
      " private:\n"
      "  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS({stub});\n"
      "  ::google::protobuf::RpcChannel* channel_;\n"
      "  bool owns_channel_;\n"
      "}};\n"
      "\n",
      fmt::arg("stub", GetBasicStubName(service)),
      fmt::arg("service", GetBasicServiceName(service)),
      fmt::arg("methods", Replace(Join(method_decls, "\n"), "\n", "\n  ")));

  *writer->NewInsertionToSource(kInsertionPointNamespaceScope) = Format(
      "{stub}::{stub}(::google::protobuf::RpcChannel* channel)\n"
      "  : channel_(channel), owns_channel_(false) {{}}\n"
      "\n"
      "{stub}::{stub}(\n"
      "    ::google::protobuf::RpcChannel* channel,\n"
      "    ::google::protobuf::Service::ChannelOwnership ownership)\n"
      "  : channel_(channel),\n"
      "    owns_channel_(ownership ==\n"
      "    ::google::protobuf::Service::STUB_OWNS_CHANNEL) {{}}\n"
      "\n"
      "{stub}::~{stub}() {{\n"
      "  if (owns_channel_) delete channel_;\n"
      "}}\n"
      "\n",
      fmt::arg("stub", GetBasicStubName(service)));
  for (int i = 0; i != service->method_count(); ++i) {
    auto&& method = service->method(i);
    *writer->NewInsertionToSource(kInsertionPointNamespaceScope) = Format(
        "void {stub}::{method}(\n"
        "    ::google::protobuf::RpcController* controller,\n"
        "    const {input_type}* request,\n"
        "    {output_type}* response,\n"
        "    ::google::protobuf::Closure* done) {{\n"
        "  channel_->CallMethod(\n"
        "      flare_rpc::GetServiceDescriptor({svc_idx})->method({index}),\n"
        "      controller, request, response, done);\n"
        "}}\n"
        "\n",
        fmt::arg("stub", GetBasicStubName(service)),
        fmt::arg("method", method->name()),
        fmt::arg("input_type", GetInputType(method)),
        fmt::arg("output_type", GetOutputType(method)),
        fmt::arg("service", GetBasicServiceName(service)),
        fmt::arg("svc_idx", service->index()),
        fmt::arg("index", method->index()));
  }
}

}  // namespace flare::protobuf::plugin
