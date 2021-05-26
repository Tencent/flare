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

#include "flare/rpc/protocol/http/builtin/misc_handler.h"

#include <string>

#include "thirdparty/googletest/gtest/gtest.h"
#include "thirdparty/jsoncpp/json.h"

#include "flare/base/down_cast.h"

namespace flare::rpc::builtin {

TEST(MiscHandler, MethodNotAllowed) {
  HttpRequest req;
  req.set_method(HttpMethod::Post);
  req.set_uri("/inspect/version");

  MiscHandler handler(nullptr);
  HttpResponse resp;
  handler.HandleRequest(req, &resp, {});
  ASSERT_EQ(HttpStatus::MethodNotAllowed, resp.status());
}

TEST(MiscHandler, Version) {
  HttpRequest req;
  req.set_method(HttpMethod::Get);
  req.set_uri("/inspect/version");

  MiscHandler handler(nullptr);
  HttpResponse resp;
  handler.HandleRequest(req, &resp, {});
  ASSERT_NE(std::string::npos, resp.body()->find("BuildTime"));
}

TEST(MiscHandler, Status) {
  HttpRequest req;
  req.set_method(HttpMethod::Get);
  req.set_uri("/inspect/status");

  MiscHandler handler(nullptr);
  HttpResponse resp;
  handler.HandleRequest(req, &resp, {});
  Json::Reader jsr;
  Json::Value jsv;
  CHECK(jsr.parse(*resp.body(), jsv));
  ASSERT_TRUE(jsv["process"]["start_time"].isString());
  ASSERT_EQ("SERVER_STATUS_OK", jsv["status"].asString());
}

}  // namespace flare::rpc::builtin
