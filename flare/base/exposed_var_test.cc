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

#include "flare/base/exposed_var.h"

#include "thirdparty/googletest/gtest/gtest.h"
#include "thirdparty/jsoncpp/json.h"

#include "flare/base/write_mostly/metrics.h"
#include "flare/testing/main.h"

namespace flare {

ExposedVarGroup* GetFancyGroup();

Json::Value GetTree();

// '/v1`
ExposedVar<int> v1("v1", 5);

// `/x/y/z/f1`.
ExposedVar<double> f1("f1", 6.2, ExposedVarGroup::FindOrCreate("/x/y/z"));

auto GreatGroup = ExposedVarGroup::FindOrCreate("/a/b");

// `/a/b/ds1`
ExposedVarDynamic<std::string> ds1(
    "ds1", [] { return "test_str"; }, GetFancyGroup());

// `/a/b/vdt1`
ExposedVarDynamicTree vdt1("vdt1", &GetTree, GetFancyGroup());

// `/a/(array)`.
ExposedVarDynamic<Json::Value> array1("array", [] {
  Json::Value value;
  Json::Value entry;
  entry["key"] = "value";
  value.append(entry);
  value.append(entry);
  value.append(entry);
  return value;
});

ExposedVarGroup* GetFancyGroup() {
  return ExposedVarGroup::FindOrCreate("/a/b");
}

// '/metrics`
ExposedMetrics<std::uint64_t> metrics("metrics");

Json::Value GetTree() {
  Json::Value jsv;
  jsv["dir"]["sub-dir"]["key"] = 5;
  jsv["key"] = "6";
  return jsv;
}

TEST(ExposedVar, Mutate) {
  auto opt = ExposedVarGroup::TryGet("/");
  ASSERT_TRUE(opt);
  auto&& jsv = *opt;
  ASSERT_EQ(5, jsv["v1"].asInt());
  *v1 = 6;
  jsv = *ExposedVarGroup::TryGet("/");
  ASSERT_EQ(6, jsv["v1"].asInt());
  *v1 = 5;  // Change it back.
}

TEST(ExposedVar, GetAll) {
  auto opt = ExposedVarGroup::TryGet("/");
  ASSERT_TRUE(opt);
  auto&& jsv = *opt;
  std::cout << Json::FastWriter().write(jsv) << std::endl;
  ASSERT_EQ(6.2, jsv["x"]["y"]["z"]["f1"].asDouble());
  ASSERT_EQ("test_str", jsv["a"]["b"]["ds1"].asString());
  ASSERT_EQ(5, jsv["a"]["b"]["vdt1"]["dir"]["sub-dir"]["key"].asInt());
  ASSERT_EQ("6", jsv["a"]["b"]["vdt1"]["key"].asString());
}

TEST(ExposedVar, GetMissing) {
  auto opt = ExposedVarGroup::TryGet("/a/b/v-not-existing");
  ASSERT_FALSE(opt);
}

TEST(ExposedVar, GetNormal) {
  auto opt = ExposedVarGroup::TryGet("/v1");
  ASSERT_TRUE(opt);
  auto&& jsv = *opt;
  ASSERT_EQ(5, jsv.asInt());
}

TEST(ExposedVar, GetDynamic) {
  auto opt = ExposedVarGroup::TryGet("/a/b/ds1");
  ASSERT_TRUE(opt);
  auto&& jsv = *opt;
  ASSERT_EQ("test_str", jsv.asString());
}

TEST(ExposedVar, GetIntoTreeDir) {
  auto opt = ExposedVarGroup::TryGet("/a/b/vdt1/dir/sub-dir");
  std::cout << ExposedVarGroup::TryGet("/")->toStyledString() << std::endl;
  ASSERT_TRUE(opt);
  auto&& jsv = *opt;
  ASSERT_EQ(5, jsv["key"].asInt());
}

TEST(ExposedVar, GetIntoTree) {
  auto opt = ExposedVarGroup::TryGet("/a/b/vdt1/dir/sub-dir/key");
  ASSERT_TRUE(opt);
  auto&& jsv = *opt;
  ASSERT_EQ(5, jsv.asInt());
}

TEST(ExposedVar, GetIntoTreeArray) {
  auto opt = ExposedVarGroup::TryGet("/array/0");
  ASSERT_TRUE(opt);
  auto&& jsv = *opt;
  ASSERT_EQ("value", jsv["key"].asString());
}

TEST(ExposedVar, GetEntireTree) {
  auto opt = ExposedVarGroup::TryGet("/a/b/vdt1");
  ASSERT_TRUE(opt);
  auto&& jsv = *opt;
  ASSERT_EQ("6", jsv["key"].asString());
}

TEST(ExposedVar, DynamicRemoval) {
  {
    // '/v0`
    ExposedVar<int> v0("v0");  // Defaults to 0.
    ASSERT_TRUE(ExposedVarGroup::TryGet("/v0"));
    ASSERT_EQ(0, ExposedVarGroup::TryGet("/v0")->asInt());
  }
  ASSERT_FALSE(ExposedVarGroup::TryGet("/v0"));
  {
    // '/v0` goes back to life again.
    ExposedVar<int> v0("v0");
    ASSERT_TRUE(ExposedVarGroup::TryGet("/v0"));
    ASSERT_EQ(0, ExposedVarGroup::TryGet("/v0")->asInt());
  }
  ASSERT_FALSE(ExposedVarGroup::TryGet("/v0"));
}

TEST(ExposedVar, SlashInPath) {
  ExposedVar<int> v0(R"(\/\/\/\/abc)", 10,
                     ExposedVarGroup::FindOrCreate("/path/to/var"));
  ASSERT_EQ(
      10,
      (*ExposedVarGroup::TryGet("/"))["path"]["to"]["var"]["////abc"].asInt());

  ExposedVarDynamicTree v1("v", [] {
    Json::Value jsv;
    jsv["path"]["to"]["//a-lot-of-slashes///"] = 123;
    return jsv;
  });
  ASSERT_EQ(123, (*ExposedVarGroup::TryGet(
                     "/"))["v"]["path"]["to"]["//a-lot-of-slashes///"]
                     .asInt());
  ASSERT_EQ(123,
            ExposedVarGroup::TryGet(R"(/v/path/to/\/\/a-lot-of-slashes\/\/\/)")
                ->asInt());
}

TEST(ExposedVar, Metrics) {
  metrics->Report(1);
  std::this_thread::sleep_for(std::chrono::seconds(2));
  ASSERT_EQ(1,
            (*ExposedVarGroup::TryGet("/"))["metrics"]["1min"]["max"].asInt());
}

}  // namespace flare

FLARE_TEST_MAIN
