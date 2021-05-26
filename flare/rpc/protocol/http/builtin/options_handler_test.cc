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

#include "flare/rpc/protocol/http/builtin/options_handler.h"

#include "thirdparty/gflags/gflags.h"
#include "thirdparty/googletest/gtest/gtest.h"
#include "thirdparty/jsoncpp/json.h"

#include "flare/base/option.h"

DEFINE_int32(fancy_int, 123, "");
DEFINE_string(fancy_str, "abc", "");

namespace flare::rpc::builtin {

GflagsOptions<std::string> opt_str("fancy_str");
GflagsOptions<int> opt_int32("fancy_int");

class OptionsHandlerTest : public ::testing::Test {
 public:
  void SetUp() override { option::InitializeOptions(); }

  void TearDown() override { option::ShutdownOptions(); }

 protected:
  std::optional<std::string> GetOptionsText(const std::string& uri) {
    HttpRequest req;
    req.set_method(HttpMethod::Get);
    req.headers()->Append("Accept-Type", "application/json");
    req.set_uri(uri);

    HttpResponse response;
    handler_.OnGet(req, &response, {});
    if (response.status() != HttpStatus::OK) {
      return {};
    }
    return *response.body();
  }
  std::optional<Json::Value> GetOptions(const std::string& uri) {
    auto opt = GetOptionsText(uri);
    if (!opt) {
      return {};
    }

    Json::Value jsv;
    FLARE_CHECK(Json::Reader().parse(*opt, jsv));
    return jsv;
  }

 private:
  OptionsHandler handler_{"/inspect/options"};
};

TEST_F(OptionsHandlerTest, GetAllOption) {
  auto jsv1 = GetOptions("/inspect/options/");
  auto jsv2 = GetOptions("/inspect/options");
  ASSERT_EQ(jsv1->toStyledString(), jsv2->toStyledString());
  ASSERT_EQ("abc", (*jsv1)["gflags"]["fancy_str"].asString());
}

TEST_F(OptionsHandlerTest, GetSomeOption) {
  auto jsv = GetOptions("/inspect/options/gflags");
  ASSERT_EQ("abc", (*jsv)["fancy_str"].asString());
}

TEST_F(OptionsHandlerTest, GetOneOption) {
  ASSERT_EQ("abc", GetOptionsText("/inspect/options/gflags/fancy_str"));
  ASSERT_EQ("123", GetOptionsText("/inspect/options/gflags/fancy_int"));
}

TEST_F(OptionsHandlerTest, Error) {
  ASSERT_FALSE(GetOptionsText("/inspect/options/gflags/fancy_str/"));
  ASSERT_FALSE(GetOptionsText("/inspect/options/gflags/boring_str"));
}

}  // namespace flare::rpc::builtin
