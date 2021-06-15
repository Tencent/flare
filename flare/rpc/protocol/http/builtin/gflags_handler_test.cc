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

#include "flare/rpc/protocol/http/builtin/gflags_handler.h"

#include "gflags/gflags.h"
#include "gtest/gtest.h"
#include "jsoncpp/json.h"

DEFINE_int64(builtin_gflags_ut_flag_1, 1234, "UT flag");
DEFINE_int64(builtin_gflags_ut_flag_2, 4567, "UT flag");

namespace flare::rpc::builtin {

TEST(GflagsHandler, GetFlagsSelectively) {
  HttpRequest req;
  req.set_method(HttpMethod::Get);
  req.headers()->Append("Accept-Type", "application/json");
  req.set_uri(
      "/inspect/gflags?name=builtin_gflags_ut_flag_1,builtin_gflags_ut_flag_2");

  GflagsHandler handler;
  HttpResponse resp;
  handler.HandleRequest(req, &resp, {});
  ASSERT_EQ(HttpStatus::OK, resp.status());

  Json::Value jsv;
  CHECK(Json::Reader().parse(*resp.body(), jsv));
  ASSERT_EQ(jsv["builtin_gflags_ut_flag_1"]["current_value"].asString(),
            std::to_string(FLAGS_builtin_gflags_ut_flag_1));
  ASSERT_EQ(jsv["builtin_gflags_ut_flag_2"]["current_value"].asString(),
            std::to_string(FLAGS_builtin_gflags_ut_flag_2));
}

TEST(GflagsHandler, GetFlagsAll) {
  HttpRequest req;
  req.set_method(HttpMethod::Get);
  req.headers()->Append("Accept-Type", "application/json");
  req.set_uri("/inspect/gflags");  // All flags.

  GflagsHandler handler;
  HttpResponse resp;
  handler.HandleRequest(req, &resp, {});
  ASSERT_EQ(HttpStatus::OK, resp.status());

  Json::Value jsv;
  CHECK(Json::Reader().parse(*resp.body(), jsv));
  ASSERT_GT(jsv.size(), 2);
  ASSERT_EQ(jsv["builtin_gflags_ut_flag_1"]["current_value"].asString(),
            std::to_string(FLAGS_builtin_gflags_ut_flag_1));
  ASSERT_EQ(jsv["builtin_gflags_ut_flag_2"]["current_value"].asString(),
            std::to_string(FLAGS_builtin_gflags_ut_flag_2));
}

TEST(GflagsHandler, SetFlags) {
  Json::Value flags;
  flags["builtin_gflags_ut_flag_1"] = 8888;
  flags["builtin_gflags_ut_flag_2"] = 9999;

  HttpRequest req;
  req.set_method(HttpMethod::Post);
  req.set_uri("/inspect/gflags");
  req.set_body(Json::FastWriter().write(flags));

  GflagsHandler handler;
  HttpResponse resp;
  handler.HandleRequest(req, &resp, {});
  ASSERT_EQ(HttpStatus::OK, resp.status());
  ASSERT_EQ("", *resp.body());

  ASSERT_EQ(8888, FLAGS_builtin_gflags_ut_flag_1);
  ASSERT_EQ(9999, FLAGS_builtin_gflags_ut_flag_2);
}

}  // namespace flare::rpc::builtin
