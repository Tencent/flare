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

#include "flare/rpc/protocol/http/builtin/exposed_vars_handler.h"

#include "gtest/gtest.h"
#include "jsoncpp/json.h"

#include "flare/base/exposed_var.h"

namespace flare::rpc::builtin {

ExposedVar<double> f1("f1", 123.456);

TEST(ExposedVarsHandler, GetAll) {
  HttpRequest req;
  req.set_method(HttpMethod::Get);
  req.set_uri("/inspect/vars");

  ExposedVarsHandler handler("/inspect/vars");
  HttpResponse resp;
  handler.HandleRequest(req, &resp, {});
  ASSERT_EQ(HttpStatus::OK, resp.status());

  Json::Value jsv;
  CHECK(Json::Reader().parse(*resp.body(), jsv));
  ASSERT_NEAR(123.456, jsv["f1"].asDouble(), 1e-7);
}

TEST(ExposedVarsHandler, GetVar) {
  HttpRequest req;
  req.set_method(HttpMethod::Get);
  req.set_uri("/inspect/vars/f1/");

  ExposedVarsHandler handler("/inspect/vars");
  HttpResponse resp;
  handler.HandleRequest(req, &resp, {});
  ASSERT_EQ(HttpStatus::OK, resp.status());

  Json::Value jsv;
  CHECK(Json::Reader().parse(*resp.body(), jsv));
  ASSERT_NEAR(123.456, jsv.asDouble(), 1e-7);
}

}  // namespace flare::rpc::builtin
