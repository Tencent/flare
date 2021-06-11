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

#include "flare/net/cos/ops/object/delete_object.h"

#include "googletest/gtest/gtest.h"

namespace flare {

TEST(DeleteObject, Request) {
  CosDeleteObjectRequest req;
  req.key = "my key";
  req.version_id = "version";

  cos::CosTask::Options opts = {.region = "region1", .bucket = "bucket2"};
  cos::CosTask task(&opts);
  ASSERT_TRUE(
      static_cast<cos::CosOperation*>(&req)->PrepareTask(&task, nullptr));

  EXPECT_EQ(HttpMethod::Delete, task.method());
  EXPECT_EQ(
      "https://bucket2.cos.region1.myqcloud.com/my%20key?versionId=version&",
      task.uri());
}

TEST(DeleteObject, result) {
  CosDeleteObjectResult result;
  ASSERT_TRUE(static_cast<cos::CosOperationResult*>(&result)->ParseResult(
      cos::CosTaskCompletion(
          HttpStatus::OK, HttpVersion::V_1_1,
          {"x-cos-version-id: ver1", "x-cos-delete-marker: true"},
          flare::CreateBufferSlow("file body")),
      {}));
  EXPECT_EQ("ver1", result.version_id);
  EXPECT_TRUE(result.delete_marker);
}

}  // namespace flare
