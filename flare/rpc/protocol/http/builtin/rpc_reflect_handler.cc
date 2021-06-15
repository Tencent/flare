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

#include "flare/rpc/protocol/http/builtin/rpc_reflect_handler.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/message.h"

#include "flare/rpc/protocol/protobuf/gdt_json_proto_conversion.h"
#include "flare/rpc/protocol/protobuf/service_method_locator.h"

FLARE_RPC_SERVER_REGISTER_BUILTIN_HTTP_PREFIX_HANDLER(
    [](auto&& owner) {
      return std::make_unique<flare::rpc::builtin::RpcReflectHandler>();
    },
    "/inspect/rpc_reflect");

namespace flare::rpc::builtin {
namespace {

// Get all field descriptor of message, include extensions.
void GetAllFieldDescriptors(
    const google::protobuf::Descriptor* descriptor,
    std::vector<const google::protobuf::FieldDescriptor*>* fields) {
  for (int i = 0; i < descriptor->field_count(); ++i) {
    fields->push_back(descriptor->field(i));
  }
  google::protobuf::MessageFactory* message_factory =
      google::protobuf::MessageFactory::generated_factory();
  const google::protobuf::Message* message =
      message_factory->GetPrototype(descriptor);
  const google::protobuf::Reflection* reflection = message->GetReflection();
  for (int i = 0; i < descriptor->extension_range_count(); ++i) {
    const google::protobuf::Descriptor::ExtensionRange* ext_range =
        descriptor->extension_range(i);
    for (int tag_number = ext_range->start; tag_number < ext_range->end;
         ++tag_number) {
      const google::protobuf::FieldDescriptor* field =
          reflection->FindKnownExtensionByNumber(tag_number);
      if (field) fields->push_back(field);
    }
  }
}

bool FillEnumInfo(const google::protobuf::EnumDescriptor* enum_descriptor,
                  Json::Value* response) {
  Json::Value root;
  root["full_name"] = enum_descriptor->full_name();
  google::protobuf::EnumDescriptorProto proto;
  enum_descriptor->CopyTo(&proto);
  Json::Value json_info;
  std::string error;
  bool succ = protobuf::ProtoMessageToJsonValue(proto, &json_info, &error);
  if (!succ) {
    FLARE_LOG_ERROR("Fail to convert json_info message to json name error {}",
                    error);
    return false;
  }
  root["info"] = json_info;
  (*response)["enum_type"].append(root);
  return true;
}

bool FillMessageType(Json::Value* response,
                     std::set<std::string>* added_type_set,
                     const google::protobuf::Descriptor* message_descriptor) {
  Json::Value root;
  root["full_name"] = message_descriptor->full_name();
  google::protobuf::DescriptorProto proto;
  message_descriptor->CopyTo(&proto);
  Json::Value json_info;
  std::string error;
  bool succ = protobuf::ProtoMessageToJsonValue(proto, &json_info, &error);
  if (!succ) {
    FLARE_LOG_ERROR("Fail to convert json_info message to json name error {}",
                    error);
    return false;
  }
  root["info"] = json_info;

  std::vector<const google::protobuf::FieldDescriptor*> fields;
  GetAllFieldDescriptors(message_descriptor, &fields);
  bool ok = true;
  for (size_t i = 0; i < fields.size(); ++i) {
    const google::protobuf::FieldDescriptor* field = fields[i];
    if (field->is_extension()) {
      google::protobuf::FieldDescriptorProto proto;
      field->CopyTo(&proto);
      Json::Value json_field;
      std::string error;
      bool succ = protobuf::ProtoMessageToJsonValue(proto, &json_field, &error);
      if (!succ) {
        FLARE_LOG_ERROR(
            "Fail to convert json_field message to json name error {}", error);
        return false;
      }
      root["info"]["field"].append(json_field);
    }

    if (field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_ENUM) {
      const google::protobuf::EnumDescriptor* enum_descriptor =
          field->enum_type();
      if (added_type_set->insert(enum_descriptor->full_name()).second)
        ok &= FillEnumInfo(enum_descriptor, response);
    } else if (field->cpp_type() ==
               google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
      const google::protobuf::Descriptor* message_type = field->message_type();
      if (added_type_set->insert(message_type->full_name()).second)
        ok &= FillMessageType(response, added_type_set, message_type);
    }
  }
  (*response)["message_type"].append(root);
  return ok;
}

bool FillAllTypesOfRequest(const google::protobuf::MethodDescriptor* method,
                           Json::Value* response) {
  google::protobuf::MethodDescriptorProto proto;
  method->CopyTo(&proto);
  Json::Value json_method;
  std::string error;
  bool succ = protobuf::ProtoMessageToJsonValue(proto, &json_method, &error);
  if (!succ) {
    FLARE_LOG_ERROR(
        "Fail to convert MethodDescriptorProto message to json name error {}",
        error);
    return false;
  }
  (*response)["method"] = json_method;
  const google::protobuf::Descriptor* descriptor = method->input_type();
  std::set<std::string> added_type_set;
  added_type_set.insert(descriptor->full_name());
  return FillMessageType(response, &added_type_set, descriptor);
}

}  // namespace

void RpcReflectHandler::OnGet(const HttpRequest& request,
                              HttpResponse* response,
                              HttpServerContext* context) {
  auto&& uri = request.uri();
  if (uri == "/inspect/rpc_reflect/services") {
    return GetServices(request, response, context);
  } else {
    const std::string kMethodUriPrefix = "/inspect/rpc_reflect/method/";
    if (uri.find(kMethodUriPrefix) == 0) {
      return GetMethod(uri.substr(kMethodUriPrefix.size()), request, response,
                       context);
    }
  }
  response->set_status(HttpStatus::NotFound);
}

void RpcReflectHandler::GetServices(const HttpRequest& request,
                                    HttpResponse* response,
                                    HttpServerContext* context) {
  auto&& services =
      protobuf::ServiceMethodLocator::Instance()->GetAllServices();
  Json::Value json_services;
  for (auto&& service : services) {
    google::protobuf::ServiceDescriptorProto proto;
    service->CopyTo(&proto);
    Json::Value json_info;
    std::string error;
    bool succ = protobuf::ProtoMessageToJsonValue(proto, &json_info, &error);
    if (!succ) {
      FLARE_LOG_ERROR("Fail to convert proto message to json name {} error {}",
                      service->full_name(), error);
      response->set_status(HttpStatus::InternalServerError);
    }
    Json::Value json_service;
    json_service["full_name"] = service->full_name();
    json_service["info"] = json_info;
    json_services.append(json_service);
  }
  Json::Value root;
  root["service"] = json_services;
  response->set_body(root.toStyledString());
  response->headers()->Append("Content-Type", "application/json");
  response->set_status(HttpStatus::OK);
}

void RpcReflectHandler::GetMethod(const std::string& method_name,
                                  const HttpRequest& request,
                                  HttpResponse* response,
                                  HttpServerContext* context) {
  auto&& services =
      protobuf::ServiceMethodLocator::Instance()->GetAllServices();
  for (auto&& service : services) {
    for (int j = 0; j < service->method_count(); ++j) {
      auto&& method = service->method(j);
      if (method->full_name() == method_name) {
        Json::Value j;
        if (!FillAllTypesOfRequest(method, &j)) {
          response->set_status(HttpStatus::InternalServerError);
          return;
        } else {
          response->set_body(j.toStyledString());
          response->headers()->Append("Content-Type", "application/json");
          response->set_status(HttpStatus::OK);
          return;
        }
      }
    }
  }
  FLARE_LOG_WARNING("Service method {} not found.", method_name);
  response->set_status(HttpStatus::NotFound);
}

}  // namespace flare::rpc::builtin
