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

#include "flare/base/option.h"

#include <thread>
#include <vector>

#include "gflags/gflags.h"
#include "googletest/gtest/gtest.h"

#include "flare/base/option/json_parser.h"
#include "flare/base/option/option_provider.h"
#include "flare/base/option/option_service.h"
#include "flare/init.h"

DEFINE_int32(int_test, 1, "");
DEFINE_int32(int_test2, 2, "");
DEFINE_int32(int_test3, 3, "");
DEFINE_string(string_test, "abc", "");
DEFINE_string(will_be_parsed_to_json, R"({"key":"value"})", "");

DECLARE_int32(flare_option_update_interval);

DECLARE_string(flare_option_feature_flag_app);
DECLARE_string(flare_option_feature_flag_env);
DECLARE_string(flare_option_feature_flag_app_secret);
DECLARE_string(flare_option_feature_flag_context);

DECLARE_string(flare_option_rainbow_app_id);

using namespace std::literals;

namespace flare {

class AlwaysFailedProvider : public OptionPassiveProvider {
 public:
  bool GetAll(const std::vector<const option::MultiKey*>& names) override {
    return false;
  }
  bool GetBool(const option::MultiKey& name, bool* value) override {
    return false;
  }
  bool GetInt8(const option::MultiKey& name, std::int8_t* value) override {
    return false;
  }
  bool GetUInt8(const option::MultiKey& name, std::uint8_t* value) override {
    return false;
  }
  bool GetInt16(const option::MultiKey& name, std::int16_t* value) override {
    return false;
  }
  bool GetUInt16(const option::MultiKey& name, std::uint16_t* value) override {
    return false;
  }
  bool GetInt32(const option::MultiKey& name, std::int32_t* value) override {
    return false;
  }
  bool GetUInt32(const option::MultiKey& name, std::uint32_t* value) override {
    return false;
  }
  bool GetInt64(const option::MultiKey& name, std::int64_t* value) override {
    return false;
  }
  bool GetUInt64(const option::MultiKey& name, std::uint64_t* value) override {
    return false;
  }
  bool GetFloat(const option::MultiKey& name, float* value) override {
    return false;
  }
  bool GetDouble(const option::MultiKey& name, double* value) override {
    return false;
  }
  bool GetString(const option::MultiKey& name, std::string* value) override {
    return false;
  }
};

FLARE_OPTION_REGISTER_OPTION_PROVIDER("always-failed-prov",
                                      AlwaysFailedProvider);

GflagsOptions<int> opt_int("int_test");
GflagsOptions<std::string> opt_str("string_test");
GflagsOptions<std::string, option::JsonParser> opt_json(
    "will_be_parsed_to_json");

TEST(Option, GFlags) {
  ASSERT_EQ(1, opt_int);
  ASSERT_EQ("abc", opt_str);
  FLAGS_int_test = 2;
  FLAGS_string_test = "def";
  option::SynchronizeOptions();
  ASSERT_EQ(2, opt_int);
  ASSERT_EQ("def", opt_str);
}

TEST(Option, GFlagsWithParser) {
  ASSERT_EQ("value", opt_json.Get()["key"].asString());
}

TEST(Option, GFlagsWithDynamicKey) {
  GflagsOptions<int> opt_int(option::DynamicKey("name"));
  option::SetDynamicKey("name", "int_test2");
  option::SynchronizeOptions();
  ASSERT_EQ(2, opt_int);
  option::SetDynamicKey("name", "int_test3");
  option::SynchronizeOptions();
  ASSERT_EQ(3, opt_int);
}

TEST(Option, ImplicitConversion) {
  std::string s = opt_str;
  std::string sv = opt_str;
  // No assertion here. So long as it compiles, we're fine.
}

TEST(Option, UsingDefaultValue) {
  Option<int> option("always-failed-prov", "meaningless-key", 12345);
  ASSERT_EQ(12345, option);
}

}  // namespace flare

int main(int argc, char** argv) {
  FLAGS_flare_option_update_interval = 1;
  flare::InitializeBasicRuntime();
  flare::option::InitializeOptions();
  ::testing::InitGoogleTest(&argc, argv);
  flare::option::ShutdownOptions();
  flare::TerminateBasicRuntime();
  return RUN_ALL_TESTS();
}
