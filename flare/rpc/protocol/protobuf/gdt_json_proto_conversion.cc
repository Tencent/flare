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

#include "flare/rpc/protocol/protobuf/gdt_json_proto_conversion.h"

#include <array>
#include <string_view>
#include <vector>

#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "jsoncpp/reader.h"
#include "jsoncpp/writer.h"

#include "flare/base/string.h"

// Copied from `common/encoding/...`, with some changes.

using namespace std::literals;

namespace flare::protobuf {

#define SET_ERROR_INFO(error_var, error_val) \
  do {                                       \
    if (error_var) *error_var = error_val;   \
  } while (0)

namespace {

constexpr std::array<bool, 256> kPctEncodingUnchanged = []() {
  constexpr auto chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789!'()*-._~"sv;
  std::array<bool, 256> rc{};
  for (auto&& e : rc) {
    e = false;
  }
  for (auto&& e : chars) {
    rc[e] = true;
  }
  return rc;
}();
constexpr std::array<bool, 256> kHexChars = []() {
  constexpr auto chars = "0123456789abcdefABCDEF"sv;
  std::array<bool, 256> rc{};
  for (auto&& e : rc) {
    e = false;
  }
  for (auto&& e : chars) {
    rc[e] = true;
  }
  return rc;
}();

inline char CharToHex(uint8_t x) { return "0123456789ABCDEF"[x]; }

std::string PctEncoded(const std::string_view& input) {
  std::string result;

  for (size_t i = 0; i < input.size(); ++i) {
    if (kPctEncodingUnchanged[static_cast<std::uint8_t>(input[i])]) {
      result.push_back(input[i]);
    } else {
      result.push_back('%');
      result.push_back(CharToHex(static_cast<std::uint8_t>(input[i]) >> 4));
      result.push_back(CharToHex(static_cast<std::uint8_t>(input[i]) % 16));
    }
  }
  return result;
}

int HexValue(uint8_t x) {
  uint8_t X = ::toupper(x);
  return (x >= '0' && x <= '9') ? X - '0' : X - 'A' + 10;
}

void PctDecodeBestEffort(std::string* str) {
  std::string& s = *str;
  size_t write_pos = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    uint8_t ch = 0;
    if (s[i] == '%' && i + 2 <= s.size() && kHexChars[s[i + 1]] &&
        kHexChars[s[i + 2]]) {
      ch = (HexValue(s[i + 1]) << 4);
      ch |= HexValue(s[i + 2]);
      i += 2;
    } else if (s[i] == '+') {
      ch = ' ';
    } else {
      ch = s[i];
    }
    s[write_pos++] = static_cast<char>(ch);
  }
  s.resize(write_pos);
}

}  // namespace

bool ProtoMessageToJsonValue(const google::protobuf::Message& message,
                             Json::Value* json_value, std::string* error,
                             const ProtoJsonFormatOptions& options) {
  using namespace std;               // NOLINT(build/namespaces)
  using namespace google::protobuf;  // NOLINT(build/namespaces)

  if (!message.IsInitialized()) {
    if (error != nullptr) {
      *error = message.InitializationErrorString();
    }
    return false;
  }

  const Reflection* reflection = message.GetReflection();

  vector<const FieldDescriptor*> fields;
  reflection->ListFields(message, &fields);

  for (size_t i = 0; i < fields.size(); i++) {
    const FieldDescriptor* field = fields[i];
    switch (field->cpp_type()) {
#define CASE_FIELD_TYPE(cpptype, method, jsontype)                        \
  case FieldDescriptor::CPPTYPE_##cpptype: {                              \
    if (field->is_repeated()) {                                           \
      int field_size = reflection->FieldSize(message, field);             \
      for (int index = 0; index < field_size; index++) {                  \
        (*json_value)[field->name()].append(static_cast<jsontype>(        \
            reflection->GetRepeated##method(message, field, index)));     \
      }                                                                   \
    } else {                                                              \
      (*json_value)[field->name()] =                                      \
          static_cast<jsontype>(reflection->Get##method(message, field)); \
    }                                                                     \
    break;                                                                \
  }

      CASE_FIELD_TYPE(INT32, Int32, Json::Value::Int);
      CASE_FIELD_TYPE(UINT32, UInt32, Json::Value::UInt);
      CASE_FIELD_TYPE(FLOAT, Float, float);
      CASE_FIELD_TYPE(DOUBLE, Double, double);
      CASE_FIELD_TYPE(BOOL, Bool, bool);
#undef CASE_FIELD_TYPE

#define CASE_64BIT_INT_FIELD(cpptype, method, valuetype)                   \
  case FieldDescriptor::CPPTYPE_##cpptype: {                               \
    if (field->is_repeated()) {                                            \
      int field_size = reflection->FieldSize(message, field);              \
      for (int index = 0; index < field_size; index++) {                   \
        valuetype number_value =                                           \
            reflection->GetRepeated##method(message, field, index);        \
        (*json_value)[field->name()].append(std::to_string(number_value)); \
      }                                                                    \
    } else {                                                               \
      valuetype number_value = reflection->Get##method(message, field);    \
      (*json_value)[field->name()] = std::to_string(number_value);         \
    }                                                                      \
    break;                                                                 \
  }

      CASE_64BIT_INT_FIELD(INT64, Int64, int64_t);
      CASE_64BIT_INT_FIELD(UINT64, UInt64, uint64_t);
#undef CASE_64BIT_INT_FIELD

      case FieldDescriptor::CPPTYPE_STRING: {
        string field_value;
        if (field->is_repeated()) {
          int field_size = reflection->FieldSize(message, field);
          for (int index = 0; index < field_size; index++) {
            const string& value = reflection->GetRepeatedStringReference(
                message, field, index, &field_value);
            const string* value_ptr = &value;
            string encode_value;
            bool is_encode = (field->type() == FieldDescriptor::TYPE_BYTES)
                                 ? options.bytes_urlencoded
                                 : options.string_urlencoded;
            if (is_encode) {
              encode_value = PctEncoded(value);
              value_ptr = &encode_value;
            }
            (*json_value)[field->name()].append(*value_ptr);
          }
        } else {
          const string& value =
              reflection->GetStringReference(message, field, &field_value);
          const string* value_ptr = &value;
          string encode_value;
          bool is_encode = (field->type() == FieldDescriptor::TYPE_BYTES)
                               ? options.bytes_urlencoded
                               : options.string_urlencoded;
          if (is_encode) {
            encode_value = PctEncoded(value);
            value_ptr = &encode_value;
          }
          (*json_value)[field->name()] = *value_ptr;
        }
        break;
      }
      case FieldDescriptor::CPPTYPE_ENUM: {
        if (field->is_repeated()) {
          int field_size = reflection->FieldSize(message, field);
          for (int index = 0; index < field_size; index++) {
            if (options.use_enum_name) {
              (*json_value)[field->name()].append(
                  reflection->GetRepeatedEnum(message, field, index)->name());
            } else {
              (*json_value)[field->name()].append(
                  reflection->GetRepeatedEnum(message, field, index)->number());
            }
          }
        } else {
          if (options.use_enum_name) {
            (*json_value)[field->name()] =
                reflection->GetEnum(message, field)->name();
          } else {
            (*json_value)[field->name()] =
                reflection->GetEnum(message, field)->number();
          }
        }
        break;
      }
      case FieldDescriptor::CPPTYPE_MESSAGE: {
        if (field->is_repeated()) {
          int field_size = reflection->FieldSize(message, field);
          for (int index = 0; index < field_size; index++) {
            Json::Value value;
            if (!ProtoMessageToJsonValue(
                    reflection->GetRepeatedMessage(message, field, index),
                    &value, error, options)) {
              return false;
            }
            (*json_value)[field->name()].append(value);
          }
        } else {
          Json::Value value;
          if (!ProtoMessageToJsonValue(reflection->GetMessage(message, field),
                                       &value, error, options)) {
            return false;
          }
          (*json_value)[field->name()] = value;
        }
        break;
      }
    }
  }
  return true;
}

bool JsonValueToProtoMessage(const Json::Value& json_value,
                             google::protobuf::Message* message,
                             std::string* error,
                             const ProtoJsonFormatOptions& options) {
  if (json_value.type() != Json::objectValue) {
    SET_ERROR_INFO(error, "type of json_value is not object.");
    return false;
  }

  using namespace std;               // NOLINT(build/namespaces)
  using namespace google::protobuf;  // NOLINT(build/namespaces)

  const Reflection* reflection = message->GetReflection();
  const Descriptor* descriptor = message->GetDescriptor();

  vector<const FieldDescriptor*> fields;
  for (int i = 0; i < descriptor->extension_range_count(); i++) {
    const Descriptor::ExtensionRange* ext_range =
        descriptor->extension_range(i);
    for (int tag_number = ext_range->start; tag_number < ext_range->end;
         tag_number++) {
      const FieldDescriptor* field =
          reflection->FindKnownExtensionByNumber(tag_number);
      if (!field) continue;
      fields.push_back(field);
    }
  }
  for (int i = 0; i < descriptor->field_count(); i++) {
    fields.push_back(descriptor->field(i));
  }

  for (size_t i = 0; i < fields.size(); i++) {
    const FieldDescriptor* field = fields[i];
    Json::Value value = json_value[field->name()];

    if (value.isNull()) {
      if (field->is_required()) {
        SET_ERROR_INFO(error,
                       "missed required field " + field->full_name() + ".");
        return false;
      }
      continue;
    }
    if (field->is_repeated()) {
      if (!value.isArray()) {
        SET_ERROR_INFO(
            error, "invalid type for array field " + field->full_name() + ".");
        return false;
      }
    }

#define VALUE_TYPE_CHECK(value, jsontype)                                 \
  if (!value.is##jsontype()) {                                            \
    SET_ERROR_INFO(error,                                                 \
                   "invalid type for field " + field->full_name() + "."); \
    return false;                                                         \
  }

    switch (field->cpp_type()) {
#define CASE_NUMERIC_FIELD(cpptype, method, jsontype, valuetype)              \
  case FieldDescriptor::CPPTYPE_##cpptype: {                                  \
    if (field->is_repeated()) {                                               \
      for (int index = 0; index < static_cast<int>(value.size()); index++) {  \
        Json::Value item = value[Json::ArrayIndex(index)];                    \
        if (item.is##jsontype()) {                                            \
          reflection->Add##method(message, field, item.as##jsontype());       \
        } else if (item.isString()) {                                         \
          if (item.asString().empty()) {                                      \
          } else if (auto opt = TryParse<valuetype>(value.asString())) {      \
            reflection->Add##method(message, field, *opt);                    \
          } else {                                                            \
            SET_ERROR_INFO(                                                   \
                error, "invalid type for field " + field->full_name() + "."); \
            return false;                                                     \
          }                                                                   \
        } else {                                                              \
          SET_ERROR_INFO(error,                                               \
                         "invalid type field " + field->full_name() + ".");   \
          return false;                                                       \
        }                                                                     \
      }                                                                       \
    } else {                                                                  \
      if (value.is##jsontype()) {                                             \
        reflection->Set##method(message, field, value.as##jsontype());        \
      } else if (value.isString()) {                                          \
        if (value.asString().empty()) {                                       \
        } else if (auto opt = TryParse<valuetype>(value.asString())) {        \
          reflection->Set##method(message, field, *opt);                      \
        } else {                                                              \
          SET_ERROR_INFO(                                                     \
              error, "invalid type for field " + field->full_name() + ".");   \
          return false;                                                       \
        }                                                                     \
      } else {                                                                \
        SET_ERROR_INFO(error,                                                 \
                       "invalid type for field " + field->full_name() + "."); \
        return false;                                                         \
      }                                                                       \
    }                                                                         \
    break;                                                                    \
  }

      CASE_NUMERIC_FIELD(INT32, Int32, Int, int32_t);
      CASE_NUMERIC_FIELD(UINT32, UInt32, UInt, uint32_t);
      CASE_NUMERIC_FIELD(FLOAT, Float, Double, float);
      CASE_NUMERIC_FIELD(DOUBLE, Double, Double, double);
      CASE_NUMERIC_FIELD(INT64, Int64, Int64, int64_t);
      CASE_NUMERIC_FIELD(UINT64, UInt64, UInt64, uint64_t);
#undef CASE_NUMERIC_FIELD

      case FieldDescriptor::CPPTYPE_BOOL: {
        if (field->is_repeated()) {
          for (int index = 0; index < static_cast<int>(value.size()); index++) {
            Json::Value item = value[Json::ArrayIndex(index)];
            VALUE_TYPE_CHECK(item, Bool);
            reflection->AddBool(message, field, item.asBool());
          }
        } else {
          VALUE_TYPE_CHECK(value, Bool);
          reflection->SetBool(message, field, value.asBool());
        }
        break;
      }

      case FieldDescriptor::CPPTYPE_STRING: {
        if (field->is_repeated()) {
          for (int index = 0; index < static_cast<int>(value.size()); index++) {
            Json::Value item = value[Json::ArrayIndex(index)];
            VALUE_TYPE_CHECK(item, String);
            string str = item.asString();
            bool is_encode = (field->type() == FieldDescriptor::TYPE_BYTES)
                                 ? options.bytes_urlencoded
                                 : options.string_urlencoded;
            if (is_encode) {
              PctDecodeBestEffort(&str);
            }
            reflection->AddString(message, field, str);
          }
        } else {
          VALUE_TYPE_CHECK(value, String);
          string str = value.asString();
          bool is_encode = (field->type() == FieldDescriptor::TYPE_BYTES)
                               ? options.bytes_urlencoded
                               : options.string_urlencoded;
          if (is_encode) {
            PctDecodeBestEffort(&str);
          }
          reflection->SetString(message, field, str);
        }
        break;
      }

      case FieldDescriptor::CPPTYPE_ENUM: {
        if (field->is_repeated()) {
          for (int index = 0; index < static_cast<int>(value.size()); index++) {
            Json::Value item = value[Json::ArrayIndex(index)];
            const EnumValueDescriptor* enum_value_descriptor = nullptr;
            if (item.isString()) {
              enum_value_descriptor =
                  field->enum_type()->FindValueByName(item.asString());
              if (!enum_value_descriptor) {
                //  try to find value by converting name to int
                if (auto opt = TryParse<int>(item.asString())) {
                  enum_value_descriptor =
                      field->enum_type()->FindValueByNumber(*opt);
                }
              }
            } else if (item.isInt()) {
              enum_value_descriptor =
                  field->enum_type()->FindValueByNumber(item.asInt());
            } else {
              SET_ERROR_INFO(
                  error, "invalid type for field " + field->full_name() + ".");
              return false;
            }

            if (!enum_value_descriptor) {
              SET_ERROR_INFO(error, "invalid value for enum field " +
                                        field->full_name() + ".");
              return false;
            }
            reflection->AddEnum(message, field, enum_value_descriptor);
          }
        } else {
          const EnumValueDescriptor* enum_value_descriptor = nullptr;
          if (value.isString()) {
            enum_value_descriptor =
                field->enum_type()->FindValueByName(value.asString());
            if (!enum_value_descriptor) {
              //  try to find value by converting name to int
              if (auto opt = TryParse<int>(value.asString())) {
                enum_value_descriptor =
                    field->enum_type()->FindValueByNumber(*opt);
              }
            }
          } else if (value.isInt()) {
            enum_value_descriptor =
                field->enum_type()->FindValueByNumber(value.asInt());
          } else {
            SET_ERROR_INFO(
                error, "invalid type for field " + field->full_name() + ".");
            return false;
          }

          if (!enum_value_descriptor) {
            SET_ERROR_INFO(error, "invalid value for enum field " +
                                      field->full_name() + ".");
            return false;
          }
          reflection->SetEnum(message, field, enum_value_descriptor);
        }
        break;
      }

      case FieldDescriptor::CPPTYPE_MESSAGE: {
        if (field->is_repeated()) {
          for (int index = 0; index < static_cast<int>(value.size()); index++) {
            Json::Value item = value[Json::ArrayIndex(index)];
            if (item.isObject()) {
              if (!JsonValueToProtoMessage(
                      item, reflection->AddMessage(message, field), error,
                      options))
                return false;

            } else {
              SET_ERROR_INFO(
                  error, "invalid type for field " + field->full_name() + ".");
              return false;
            }
          }
        } else {
          if (!JsonValueToProtoMessage(
                  value, reflection->MutableMessage(message, field), error,
                  options))
            return false;
        }
        break;
      }
    }
  }
  return true;
}

bool JsonToProtoMessage(const std::string_view& json_string_piece,
                        google::protobuf::Message* message, std::string* error,
                        const ProtoJsonFormatOptions& options) {
  Json::Value root;
  Json::Reader reader;
  if (!reader.parse(json_string_piece.data(),
                    json_string_piece.data() + json_string_piece.size(),
                    root)) {
    SET_ERROR_INFO(error, "json string format error.");
    return false;
  }

  return JsonValueToProtoMessage(root, message, error, options);
}

bool ProtoMessageToJson(const google::protobuf::Message& message,
                        std::string* json_string, std::string* error,
                        const ProtoJsonFormatOptions& options) {
  Json::Value root;
  root = Json::objectValue;
  if (ProtoMessageToJsonValue(message, &root, error, options)) {
    if (options.enable_styled) {
      Json::StyledWriter styled_writer;
      *json_string = styled_writer.write(root);
    } else {
      Json::FastWriter fast_writer;
      *json_string = fast_writer.write(root);
    }
    return true;
  }
  return false;
}

}  // namespace flare::protobuf
