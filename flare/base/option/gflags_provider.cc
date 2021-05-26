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

#include "flare/base/option/option_provider.h"

#include "thirdparty/gflags/gflags.h"

#include "flare/base/string.h"

namespace flare::option {

// Supports reading values from GFlags. Mostly used for testing / illustration
// purpose.
class GFlagsProvider : public OptionPassiveProvider {
 public:
  bool GetAll(const std::vector<const option::MultiKey*>& names) override {
    return true;
  }

#define DEFINE_GFLAGS_PROVIDER_IMPL_FOR(Type, TypeName, GFlagTypeName)      \
  bool Get##TypeName(const option::MultiKey& name, Type* value) override {  \
    auto&& info =                                                           \
        google::GetCommandLineFlagInfoOrDie(name.ToString().c_str());       \
    auto val_opt = TryParse<Type>(info.current_value);                      \
    /* This is absolutely a programming error, as both GFlags and */        \
    /* `Option<T>` are defined programatically. */                          \
    FLARE_CHECK(val_opt, "Failed to read flag [{}] of type [{}].",          \
                name.ToString(), #Type);                                    \
    FLARE_CHECK_EQ(info.type, GFlagTypeName, "Type mismatch on flag [{}].", \
                   name.ToString());                                        \
    *value = *val_opt;                                                      \
    return true;                                                            \
  }

  DEFINE_GFLAGS_PROVIDER_IMPL_FOR(bool, Bool, "bool")
  DEFINE_GFLAGS_PROVIDER_IMPL_FOR(std::int32_t, Int32, "int32")
  DEFINE_GFLAGS_PROVIDER_IMPL_FOR(std::int64_t, Int64, "int64")
  DEFINE_GFLAGS_PROVIDER_IMPL_FOR(std::uint64_t, UInt64, "uint64")
  DEFINE_GFLAGS_PROVIDER_IMPL_FOR(double, Double, "double")

#define DEFINE_NOT_SUPPORTED_IMPL_FOR(Type, TypeName)                      \
  bool Get##TypeName(const option::MultiKey& name, Type* value) override { \
    FLARE_CHECK(0, "Not supported: GFlags does not allow type [{}].",      \
                #TypeName);                                                \
    return false;                                                          \
  }

  DEFINE_NOT_SUPPORTED_IMPL_FOR(std::int8_t, Int8)
  DEFINE_NOT_SUPPORTED_IMPL_FOR(std::uint8_t, UInt8)
  DEFINE_NOT_SUPPORTED_IMPL_FOR(std::int16_t, Int16)
  DEFINE_NOT_SUPPORTED_IMPL_FOR(std::uint16_t, UInt16)
  DEFINE_NOT_SUPPORTED_IMPL_FOR(std::uint32_t, UInt32)
  DEFINE_NOT_SUPPORTED_IMPL_FOR(float, Float)

  bool GetString(const option::MultiKey& name, std::string* value) override {
    auto&& info = google::GetCommandLineFlagInfoOrDie(name.ToString().c_str());
    FLARE_CHECK_EQ(info.type, "string", "Type mismatch on flag [{}].",
                   name.ToString());
    *value = info.current_value;
    return true;
  }
};

FLARE_OPTION_REGISTER_OPTION_PROVIDER("gflags", GFlagsProvider);

}  // namespace flare::option
