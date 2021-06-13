// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "flare/net/cos/ops/object/get_object.h"

#include "gtest/gtest.h"

namespace flare {

TEST(GetObject, Request) {
  CosGetObjectRequest req;
  req.key = "my key";
  req.version_id = "version";
  req.traffic_limit = 838860800;

  cos::CosTask::Options opts = {.region = "region1", .bucket = "bucket2"};
  cos::CosTask task(&opts);
  ASSERT_TRUE(
      static_cast<cos::CosOperation*>(&req)->PrepareTask(&task, nullptr));

  EXPECT_EQ(HttpMethod::Get, task.method());
  EXPECT_EQ(
      "https://bucket2.cos.region1.myqcloud.com/my%20key?versionId=version&",
      task.uri());
  ASSERT_EQ(1, task.headers().size());
  EXPECT_EQ("x-cos-traffic-limit: 838860800", task.headers()[0]);
}

TEST(GetObject, result) {
  CosGetObjectResult result;
  ASSERT_TRUE(static_cast<cos::CosOperationResult*>(&result)->ParseResult(
      cos::CosTaskCompletion(
          HttpStatus::OK, HttpVersion::V_1_1,
          {"x-cos-storage-class: MAZ_INTELLIGENT_TIERING",
           "x-cos-storage-tier: INFREQUENT", "x-cos-version-id: ver1"},
          flare::CreateBufferSlow("file body")),
      {}));
  EXPECT_EQ("MAZ_INTELLIGENT_TIERING", result.storage_class);
  EXPECT_EQ("INFREQUENT", result.storage_tier);
  EXPECT_EQ("ver1", result.version_id);
  EXPECT_EQ("file body", flare::FlattenSlow(result.bytes));
}

}  // namespace flare
