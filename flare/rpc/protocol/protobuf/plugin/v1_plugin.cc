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

// Usage:
//
// protoc --plugin=protoc-gen-flare-rpc=path/to/v1_plugin --cpp_out=./
//   --flare_rpc_out=./ proto_file

#include <iostream>
#include <string>

#include "google/protobuf/compiler/code_generator.h"
#include "google/protobuf/compiler/cpp/cpp_helpers.h"
#include "google/protobuf/compiler/plugin.h"
#include "google/protobuf/compiler/plugin.pb.h"

#include "flare/base/logging.h"
#include "flare/base/string.h"
#include "flare/rpc/protocol/protobuf/plugin/async_decl_generator.h"
#include "flare/rpc/protocol/protobuf/plugin/basic_decl_generator.h"
#include "flare/rpc/protocol/protobuf/plugin/gdt_future_decl_generator.h"
#include "flare/rpc/protocol/protobuf/plugin/names.h"
#include "flare/rpc/protocol/protobuf/plugin/sync_decl_generator.h"

using namespace std::literals;

namespace flare::protobuf::plugin {

namespace {

class V1CodeWriter : public CodeWriter {
 public:
  V1CodeWriter(const google::protobuf::FileDescriptor* file_descriptor,
               google::protobuf::compiler::CodeGeneratorResponse* resp)
      : response_(resp) {
    constexpr auto kSuffix = ".proto"sv;
    filename_prefix_ = file_descriptor->name();
    if (EndsWith(filename_prefix_, kSuffix)) {
      filename_prefix_.erase(filename_prefix_.end() - kSuffix.size(),
                             filename_prefix_.end());
    }
  }

  std::string* NewInsertionToHeader(
      const std::string& insertion_point) override {
    auto added_file = response_->add_file();
    added_file->set_name(filename_prefix_ + ".pb.h");
    added_file->set_insertion_point(insertion_point);
    return added_file->mutable_content();
  }

  std::string* NewInsertionToSource(
      const std::string& insertion_point) override {
    auto added_file = response_->add_file();
    added_file->set_name(filename_prefix_ + ".pb.cc");
    added_file->set_insertion_point(insertion_point);
    return added_file->mutable_content();
  }

 private:
  std::string filename_prefix_;
  google::protobuf::compiler::CodeGeneratorResponse* response_;
};

// The generator.
//
// It's implementation is scattered in several `generate_xxx_impl.cc` since it's
// so long that putting them all in a single TU is infeasible.
class V1Generator : public google::protobuf::compiler::CodeGenerator {
 public:
  bool Generate(const google::protobuf::FileDescriptor* file,
                const std::string& parameter,
                google::protobuf::compiler::GeneratorContext* generator_context,
                std::string* error) const override;

 private:
  void GeneratePrologue(const google::protobuf::FileDescriptor* file,
                        CodeWriter* writer) const;

  void GenerateCodeFor(const google::protobuf::FileDescriptor* file,
                       const google::protobuf::ServiceDescriptor* service,
                       CodeWriter* writer) const;

  void GenerateEpilogue(const google::protobuf::FileDescriptor* file,
                        CodeWriter* writer) const;
};

bool V1Generator::Generate(
    const google::protobuf::FileDescriptor* file, const std::string& parameter,
    [[maybe_unused]] google::protobuf::compiler::GeneratorContext*,
    std::string* error) const {
  if (!file->service_count()) {
    return true;
  }
  if (file->options().cc_generic_services()) {
    // To the contrary of what we had done in our old plugins, we only generate
    // service if the user did NOT specify `cc_generic_services`. This is
    // actually what Protocol Buffers recommends.
    //
    // @sa: https://developers.google.com/protocol-buffers/docs/proto#options
    return true;
  }

  // We'll be generating code into this object. (See below for its functionality
  // and usage.)
  google::protobuf::compiler::CodeGeneratorResponse response;
  V1CodeWriter writer(file, &response);

  // Generate code.
  GeneratePrologue(file, &writer);
  for (int i = 0; i != file->service_count(); ++i) {
    GenerateCodeFor(file, file->service(i), &writer);
  }
  GenerateEpilogue(file, &writer);

  // Protocol Buffers' runtime expects a `CodeGeneratorResponse` in stdout as a
  // response.
  //
  // This is, as I see it, an implementation detail. It seems more appropriate
  // for us to call `generator_context->OpenForInsert` to insert our code.
  // However, `OpenForInsert` returns a `ZeroCopyOutputStream`, which is rather
  // weird to deal with.
  std::string rc = response.SerializeAsString();
  std::cout << rc;
  return !rc.empty();
}

void V1Generator::GeneratePrologue(const google::protobuf::FileDescriptor* file,
                                   CodeWriter* writer) const {
  // Headers.
  auto header_incls =
      "#include <utility>\n"
      "#include <google/protobuf/generated_enum_reflection.h>\n"
      "#include <google/protobuf/service.h>\n"
      "#include \"flare/base/callback.h\"\n"
      "#include \"flare/base/future.h\"\n"
      "#include \"flare/base/status.h\"\n"
      "#include \"flare/base/down_cast.h\"\n"
      "#include \"flare/base/maybe_owning.h\"\n"
      "#include \"flare/rpc/internal/stream.h\"\n"s;
  auto source_incls = header_incls +
                      "#include <mutex>\n"
                      "#include \"flare/rpc/rpc_channel.h\"\n"
                      "#include \"flare/rpc/rpc_client_controller.h\"\n"
                      "#include \"flare/rpc/rpc_server_controller.h\"\n";
  *writer->NewInsertionToHeader(kInsertionPointIncludes) = header_incls;
  *writer->NewInsertionToSource(kInsertionPointIncludes) = source_incls;

  // `RpcServerController` / `RpcClientController` brings in too much
  // dependencies, so we forward declare them so as not to bring them into the
  // header.
  *writer->NewInsertionToHeader(kInsertionPointIncludes) =
      "\n"
      "namespace flare {\n"
      "\n"
      "class RpcServerController;\n"
      "class RpcClientController;\n"
      "\n"
      "}  // namespace flare\n"
      "\n";

  // Initialize service descriptors. Indexed by service's indices.
  *writer->NewInsertionToSource(kInsertionPointNamespaceScope) = Format(
      "namespace {{\n"
      "namespace flare_rpc {{\n"
      "\n"
      "const ::google::protobuf::ServiceDescriptor*\n"
      "  file_level_service_descriptors[{service_count}];"
      "\n"
      "void InitServiceDescriptorsOnce() {{\n"
      "  static std::once_flag f;\n"
      "  std::call_once(f, [] {{\n"
      "    auto file = "
      "::google::protobuf::DescriptorPool::generated_pool()\n"
      "        ->FindFileByName(\"{file}\");\n"
      "    for (int i = 0; i != file->service_count(); ++i) {{\n"
      "      file_level_service_descriptors[i] = file->service(i);\n"
      "    }}\n"
      "  }});\n"
      "}}\n"
      "\n"
      "const ::google::protobuf::ServiceDescriptor*\n"
      "GetServiceDescriptor(int index) {{\n"
      "  InitServiceDescriptorsOnce();\n"
      "  return file_level_service_descriptors[index];\n"
      "}}\n"
      "\n"
      "}}  // namespace flare_rpc\n"
      "}}  // namespace\n"
      "\n",
      fmt::arg("service_count", file->service_count()),
      fmt::arg("file_ns", google::protobuf::compiler::cpp::FileLevelNamespace(
                              file->name())),
      fmt::arg("file", file->name()));
}

void V1Generator::GenerateCodeFor(
    const google::protobuf::FileDescriptor* file,
    const google::protobuf::ServiceDescriptor* service,
    CodeWriter* writer) const {
  // We got to generate all the service code (including the "default" ones)
  // since `cc_generic_services` is not set (otherwise we won't be here.).

  // This one is API compatible with what protobuf's generates.
  BasicDeclGenerator().GenerateService(file, service, writer);
  BasicDeclGenerator().GenerateStub(file, service, writer);

  // The synchronous one.
  SyncDeclGenerator().GenerateService(file, service, writer);
  SyncDeclGenerator().GenerateStub(file, service, writer);

  // The one uses future.
  //
  // We only generate stub here, I don't see point in implementing service in
  // asynchronous fashion (at least in Flare).
  AsyncDeclGenerator().GenerateStub(file, service, writer);

  // This one is API compatible with what's generated by `gdt_future_rpc`.
  //
  // They shouldn't be generated in open-source version of Flare as they carry
  // dependencies on code outside of Flare.
  GdtFutureDeclGenerator().GenerateService(file, service, writer);
  GdtFutureDeclGenerator().GenerateStub(file, service, writer);
}

void V1Generator::GenerateEpilogue(const google::protobuf::FileDescriptor* file,
                                   CodeWriter* writer) const {
  for (int i = 0; i != file->service_count(); ++i) {
    auto&& service = file->service(i);

    // For backward compatibility, we need to make these aliases.
    *writer->NewInsertionToHeader(kInsertionPointNamespaceScope) = Format(
        "using {} = {};\n"
        "using {} = {};\n"
        "\n",
        service->name(), GetBasicServiceName(service),
        service->name() + "_Stub", GetBasicStubName(service));
  }
}

}  // namespace

}  // namespace flare::protobuf::plugin

// To recognize service option `gdt.streaming_response`, we need either:
//
// - Link with `//common/rpc:rpc_options_proto`, which we want to avoid in the
//   first place, or
//
// - Do some dirty hack.
struct StreamingResponseInitializer {
  StreamingResponseInitializer() {
    google::protobuf::internal::ExtensionSet::RegisterExtension(
        google::protobuf::MethodOptions::internal_default_instance(), 10003, 8,
        false, false);
  }
} streaming_response_initializer;

int main(int argc, char* argv[]) {
  flare::protobuf::plugin::V1Generator gen;
  return google::protobuf::compiler::PluginMain(argc, argv, &gen);
}
