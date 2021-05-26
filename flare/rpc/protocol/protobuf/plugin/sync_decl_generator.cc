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

#include "flare/rpc/protocol/protobuf/plugin/sync_decl_generator.h"

#include <string>
#include <vector>

#include "thirdparty/protobuf/compiler/cpp/cpp_helpers.h"

#include "flare/base/string.h"
#include "flare/rpc/protocol/protobuf/plugin/names.h"
#include "flare/rpc/protocol/protobuf/rpc_options.h"

using namespace std::literals;

namespace flare::protobuf::plugin {

void SyncDeclGenerator::GenerateService(
    const google::protobuf::FileDescriptor* file,
    const google::protobuf::ServiceDescriptor* service, CodeWriter* writer) {
  // Generate service class' definition in header.
  std::vector<std::string> method_decls;
  for (int i = 0; i != service->method_count(); ++i) {
    auto&& method = service->method(i);
    bool client_streaming = IsClientStreamingMethod(method);
    bool server_streaming = IsServerStreamingMethod(method);
    std::string pattern;

    if (!client_streaming && !server_streaming) {  // Easy case.
      pattern =
          "virtual void {method}(\n"
          "    const {input_type}& request,\n"  // Ref here, not pointer.
          "    {output_type}* response,\n"
          "    ::flare::RpcServerController* controller);";
    } else if (client_streaming && !server_streaming) {
      pattern =
          "virtual void {method}(\n"
          "    ::flare::StreamReader<{input_type}> reader,\n"
          "    {output_type}* response,\n"
          "    ::flare::RpcServerController* controller);";
    } else if (!client_streaming && server_streaming) {
      pattern =
          "virtual void {method}(\n"
          "    const {input_type}& request,\n"  // Ref here, not pointer.
          "    ::flare::StreamWriter<{output_type}> writer,\n"
          "    ::flare::RpcServerController* controller);";
    } else {
      CHECK(client_streaming && server_streaming);
      pattern =
          "virtual void {method}(\n"
          "    ::flare::StreamReader<{input_type}> reader,\n"
          "    ::flare::StreamWriter<{output_type}> writer,\n"
          "    ::flare::RpcServerController* controller);";
    }
    method_decls.emplace_back() =
        Format(pattern, fmt::arg("method", method->name()),
               fmt::arg("input_type", GetInputType(method)),
               fmt::arg("output_type", GetOutputType(method)));
  }
  *writer->NewInsertionToHeader(kInsertionPointNamespaceScope) = Format(
      "class {service} : public ::google::protobuf::Service {{\n"
      " protected:\n"
      "  {service}() = default;\n"
      "\n"
      " public:\n"
      "  virtual ~{service}() = default;\n"
      "\n"
      "  {methods}\n"
      "\n"
      "  ///////////////////////////////////////////////\n"
      "  // Methods below are for internal use only.  //\n"
      "  ///////////////////////////////////////////////\n"
      "\n"
      "  const ::google::protobuf::ServiceDescriptor* GetDescriptor() final;\n"
      "\n"
      "  void CallMethod(const ::google::protobuf::MethodDescriptor* method,\n"
      "                  ::google::protobuf::RpcController* controller,\n"
      "                  const ::google::protobuf::Message* request,\n"
      "                  ::google::protobuf::Message* response,\n"
      "                  ::google::protobuf::Closure* done) override;\n"
      "\n"
      "  const ::google::protobuf::Message& GetRequestPrototype(\n"
      "      const ::google::protobuf::MethodDescriptor* method) const final;\n"
      "  const ::google::protobuf::Message& GetResponsePrototype(\n"
      "      const ::google::protobuf::MethodDescriptor* method) const final;\n"
      "\n"
      " private:\n"
      "  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS({service});\n"
      "}};\n"
      "\n",
      fmt::arg("stub", GetSyncStubName(service)),
      fmt::arg("service", GetSyncServiceName(service)),
      fmt::arg("methods", Replace(Join(method_decls, "\n"), "\n", "\n  ")));

  // Generate service's implementation.
  std::vector<std::string> call_method_impls;
  std::vector<std::string> get_request_prototype_impls;
  std::vector<std::string> get_response_prototype_impls;
  for (int i = 0; i != service->method_count(); ++i) {
    auto&& method = service->method(i);
    bool client_streaming = IsClientStreamingMethod(method);
    bool server_streaming = IsServerStreamingMethod(method);
    std::string pattern;

    if (!client_streaming && !server_streaming) {
      pattern =
          "case {index}: {{\n"
          "  {method}(\n"
          "      *::google::protobuf::down_cast<const {input_type}*>(\n"
          "          request),\n"
          "      ::google::protobuf::down_cast<{output_type}*>(response),\n"
          "      ctlr);\n"
          "  done->Run();\n"
          "  break;\n"
          "}}";
    } else if (client_streaming && !server_streaming) {
      pattern =
          "case {index}: {{\n"
          "  {method}(\n"
          "      ctlr->GetStreamReader<{input_type}>(),\n"
          "      ::google::protobuf::down_cast<{output_type}*>(response),\n"
          "      ctlr);\n"
          "  done->Run();\n"
          "  break;\n"
          "}}";
    } else if (!client_streaming && server_streaming) {
      pattern =
          "case {index}: {{\n"
          "  {method}(\n"
          "      *::google::protobuf::down_cast<const {input_type}*>(\n"
          "          request),\n"
          "      ctlr->GetStreamWriter<{output_type}>(),\n"
          "      ctlr);\n"
          "  done->Run();\n"
          "  break;\n"
          "}}";
    } else {
      CHECK(client_streaming && server_streaming);
      pattern =
          "case {index}: {{\n"
          "  {method}(\n"
          "      ctlr->GetStreamReader<{input_type}>(),\n"
          "      ctlr->GetStreamWriter<{output_type}>(),\n"
          "      ctlr);\n"
          "  done->Run();\n"
          "  break;\n"
          "}}";
    }

    call_method_impls.emplace_back() =
        Format(pattern, fmt::arg("index", method->index()),
               fmt::arg("method", method->name()),
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
      "  auto ctlr = ::flare::down_cast<flare::RpcServerController>(\n"
      "      controller);\n"
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
      fmt::arg("service", GetSyncServiceName(service)),
      fmt::arg("file_ns", google::protobuf::compiler::cpp::FileLevelNamespace(
                              file->name())),
      fmt::arg("svc_idx", service->index()),
      fmt::arg("call_method_cases",
               Replace(Join(call_method_impls, "\n"), "\n", "\n    ")),
      fmt::arg(
          "get_request_prototype_cases",
          Replace(Join(get_request_prototype_impls, "\n"), "\n", "\n    ")),
      fmt::arg(
          "get_response_prototype_cases",
          Replace(Join(get_response_prototype_impls, "\n"), "\n", "\n    ")));

  for (int i = 0; i != service->method_count(); ++i) {
    auto&& method = service->method(i);
    bool client_streaming = IsClientStreamingMethod(method);
    bool server_streaming = IsServerStreamingMethod(method);
    std::string pattern;

    if (!client_streaming && !server_streaming) {  // Easy case.
      pattern =
          "void {service}::{method}(\n"
          "    const {input_type}& request,\n"  // Ref here, not pointer.
          "    {output_type}* response,\n"
          "    ::flare::RpcServerController* controller) {{\n"
          "  {body}\n"
          "}}";
    } else if (client_streaming && !server_streaming) {
      pattern =
          "void {service}::{method}(\n"
          "    ::flare::StreamReader<{input_type}> reader,\n"
          "    {output_type}* response,\n"
          "    ::flare::RpcServerController* controller) {{\n"
          "  {body}\n"
          "  reader.Close();\n"
          "}}";
    } else if (!client_streaming && server_streaming) {
      pattern =
          "void {service}::{method}(\n"
          "    const {input_type}& request,\n"  // Ref here, not pointer.
          "    ::flare::StreamWriter<{output_type}> writer,\n"
          "    ::flare::RpcServerController* controller) {{\n"
          "  {body}\n"
          "  writer.Close();\n"
          "}}";
    } else {
      CHECK(client_streaming && server_streaming);
      pattern =
          "void {service}::{method}(\n"
          "    ::flare::StreamReader<{input_type}> reader,\n"
          "    ::flare::StreamWriter<{output_type}> writer,\n"
          "    ::flare::RpcServerController* controller) {{\n"
          "  {body}\n"
          "  reader.Close();\n"
          "  writer.Close();\n"
          "}}";
    }
    auto body = Format(
        "controller->SetFailed(\n"
        "    ::flare::rpc::STATUS_FAILED,\n"
        "    \"Method {method}() not implemented.\");",
        fmt::arg("method", method->name()));
    *writer->NewInsertionToSource(kInsertionPointNamespaceScope) =
        Format(pattern, fmt::arg("service", GetSyncServiceName(service)),
               fmt::arg("method", method->name()),
               fmt::arg("input_type", GetInputType(method)),
               fmt::arg("output_type", GetOutputType(method)),
               fmt::arg("body", body)) +
        "\n\n";
  }
}

void SyncDeclGenerator::GenerateStub(
    const google::protobuf::FileDescriptor* file,
    const google::protobuf::ServiceDescriptor* service, CodeWriter* writer) {
  // Code in header.
  std::vector<std::string> method_decls;
  for (int i = 0; i != service->method_count(); ++i) {
    auto&& method = service->method(i);
    bool client_streaming = IsClientStreamingMethod(method);
    bool server_streaming = IsServerStreamingMethod(method);
    std::string pattern;

    // TODO(luobogao): Support default controller (used when `controller` is not
    // specified or specified as `nullptr`.).
    if (!client_streaming && !server_streaming) {
      // TBH I'm not sure if we should return `Expected<T>` or `optional<T>`.
      // The former looks more appropriate but our `Expected` has not been
      // thoroughly thought about.
      pattern =
          "::flare::Expected<{output_type},\n"
          "                  ::flare::Status>\n"
          "{method}(\n"
          "    const {input_type}& request,\n"
          "    ::flare::RpcClientController* controller);";
    } else if (client_streaming && !server_streaming) {
      pattern =
          "std::pair<\n"
          "    ::flare::StreamReader<{output_type}>,\n"
          "    ::flare::StreamWriter<{input_type}>>\n"
          "{method}(::flare::RpcClientController* controller);";
    } else if (!client_streaming && server_streaming) {
      pattern =
          "::flare::StreamReader<{output_type}> {method}(\n"
          "    const {input_type}& request,\n"
          "    ::flare::RpcClientController* controller);";
    } else {
      CHECK(client_streaming && server_streaming);
      // Same as client-side streaming.
      pattern =
          "std::pair<\n"
          "    ::flare::StreamReader<{output_type}>,\n"
          "    ::flare::StreamWriter<{input_type}>>\n"
          "{method}(::flare::RpcClientController* controller);";
    }

    method_decls.emplace_back() =
        Format(pattern, fmt::arg("method", method->name()),
               fmt::arg("input_type", GetInputType(method)),
               fmt::arg("output_type", GetOutputType(method)),
               fmt::arg("index", method->index()),
               fmt::arg("service", GetSyncServiceName(service)));
  }
  *writer->NewInsertionToHeader(kInsertionPointNamespaceScope) = Format(
      "class {stub} {{\n"
      "  using MaybeOwningChannel = ::flare::MaybeOwningArgument<\n"
      "      ::google::protobuf::RpcChannel>;\n"
      " public:\n"
      "  {stub}(MaybeOwningChannel channel)\n"
      "    : channel_(std::move(channel)) {{}}\n"
      "\n"
      "  {stub}(const std::string& uri);\n"
      "\n"
      "  {methods}\n"
      "\n"
      " private:\n"
      "  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS({stub});\n"
      "  ::flare::MaybeOwning<::google::protobuf::RpcChannel> channel_;\n"
      "}};\n"
      "\n",
      fmt::arg("stub", GetSyncStubName(service)),
      fmt::arg("methods", Replace(Join(method_decls, "\n"), "\n", "\n  ")));

  *writer->NewInsertionToSource(kInsertionPointNamespaceScope) = Format(
      "{stub}::{stub}(const std::string& uri) {{\n"
      "  channel_ = std::make_unique<flare::RpcChannel>(uri);\n"
      "}}"
      "\n",
      fmt::arg("stub", GetSyncStubName(service)));

  for (int i = 0; i != service->method_count(); ++i) {
    auto&& method = service->method(i);
    bool client_streaming = IsClientStreamingMethod(method);
    bool server_streaming = IsServerStreamingMethod(method);
    std::string pattern;

    // TODO(luobogao): Support default controller (used when `controller` is not
    // specified or specified as `nullptr`.).
    if (!client_streaming && !server_streaming) {
      pattern =
          "::flare::Expected<{output_type}, ::flare::Status>\n"
          "{stub}::{method}(\n"
          "    const {input_type}& request,\n"
          "    ::flare::RpcClientController* ctlr) {{\n"
          "  {output_type} rc;\n"
          "  channel_->CallMethod(\n"
          "      flare_rpc::GetServiceDescriptor({svc_idx})->method({index}),\n"
          "      ctlr, &request, &rc, nullptr);\n"
          "  if (!ctlr->Failed()) {{\n"
          "    return rc;\n"
          "  }}\n"
          // By reaching here, the call must have failed.
          "  return flare::Status(ctlr->ErrorCode(), ctlr->ErrorText());\n"
          "}}";
    } else if (client_streaming && !server_streaming) {
      pattern =
          "std::pair<\n"
          "    ::flare::StreamReader<{output_type}>,\n"
          "    ::flare::StreamWriter<{input_type}>>\n"
          "{stub}::{method}(\n"
          "    ::flare::RpcClientController* ctlr) {{\n"
          "  channel_->CallMethod(\n"
          "      flare_rpc::GetServiceDescriptor({svc_idx})->method({index}),\n"
          "      ctlr, nullptr, nullptr, nullptr);\n"
          "  return std::pair(ctlr->GetStreamReader<{output_type}>(),\n"
          "                   ctlr->GetStreamWriter<{input_type}>());\n"
          "}}";
    } else if (!client_streaming && server_streaming) {
      pattern =
          "::flare::StreamReader<{output_type}>\n"
          "{stub}::{method}(\n"
          "    const {input_type}& request,\n"
          "    ::flare::RpcClientController* ctlr) {{\n"
          "  channel_->CallMethod(\n"
          "      flare_rpc::GetServiceDescriptor({svc_idx})->method({index}),\n"
          "      ctlr, &request, nullptr, nullptr);\n"
          "  return ctlr->GetStreamReader<{output_type}>();\n"
          "}}";
    } else {
      CHECK(client_streaming && server_streaming);
      pattern =
          "std::pair<\n"
          "    ::flare::StreamReader<{output_type}>,\n"
          "    ::flare::StreamWriter<{input_type}>>\n"
          "{stub}::{method}(\n"
          "    ::flare::RpcClientController* ctlr) {{\n"
          "  channel_->CallMethod(\n"
          "      flare_rpc::GetServiceDescriptor({svc_idx})->method({index}),\n"
          "      ctlr, nullptr, nullptr, nullptr);\n"
          "  return std::pair(ctlr->GetStreamReader<{output_type}>(),\n"
          "                   ctlr->GetStreamWriter<{input_type}>());\n"
          "}}";  // Same as client-side streaming.
    }

    *writer->NewInsertionToSource(kInsertionPointNamespaceScope) =
        Format(pattern, fmt::arg("stub", GetSyncStubName(service)),
               fmt::arg("method", method->name()),
               fmt::arg("input_type", GetInputType(method)),
               fmt::arg("output_type", GetOutputType(method)),
               fmt::arg("svc_idx", service->index()),
               fmt::arg("index", method->index()),
               fmt::arg("service", GetSyncServiceName(service))) +
        "\n\n";
  }
}

}  // namespace flare::protobuf::plugin
