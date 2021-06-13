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

#ifndef FLARE_RPC_PROTOCOL_PROTOBUF_GDT_JSON_PROTO_CONVERSION_H_
#define FLARE_RPC_PROTOCOL_PROTOBUF_GDT_JSON_PROTO_CONVERSION_H_

#include <string>
#include <string_view>

#include "jsoncpp/value.h"
#include "google/protobuf/service.h"

// Conversion between gdt JSON & Protocol Buffers messages.
//
// Copied from `common/encoding/json_to_pb.*`

namespace flare::protobuf {

struct ProtoJsonFormatOptions {
  bool string_urlencoded = false;
  bool bytes_urlencoded = true;
  bool enable_styled = false;
  bool use_enum_name = false;
};

bool ProtoMessageToJson(
    const google::protobuf::Message& message, std::string* json_string,
    std::string* error,
    const ProtoJsonFormatOptions& options = ProtoJsonFormatOptions());
bool JsonToProtoMessage(
    const std::string_view& json_string_piece,
    google::protobuf::Message* message, std::string* error,
    const ProtoJsonFormatOptions& options = ProtoJsonFormatOptions());

bool ProtoMessageToJsonValue(
    const google::protobuf::Message& message, Json::Value* json_value,
    std::string* error,
    const ProtoJsonFormatOptions& options = ProtoJsonFormatOptions());

}  // namespace flare::protobuf

#endif  // FLARE_RPC_PROTOCOL_PROTOBUF_GDT_JSON_PROTO_CONVERSION_H_
