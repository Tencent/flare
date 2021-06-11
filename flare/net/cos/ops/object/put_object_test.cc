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

#include "flare/net/cos/ops/object/put_object.h"

#include <iostream>

#include "googletest/gtest/gtest.h"

namespace flare {

TEST(PutObject, Request) {
  CosPutObjectRequest req;
  req.key = "my key";
  req.storage_class = "MAZ_INTELLIGENT_TIERING";
  req.traffic_limit = 838860800;
  req.tagging = "my tag";
  req.bytes = flare::CreateBufferSlow("bytes");

  cos::CosTask::Options opts = {.region = "region1", .bucket = "bucket2"};
  cos::CosTask task(&opts);
  ASSERT_TRUE(
      static_cast<cos::CosOperation*>(&req)->PrepareTask(&task, nullptr));

  for (auto&& e : task.headers()) {
    std::cout << e << std::endl;
  }
  EXPECT_EQ(HttpMethod::Put, task.method());
  EXPECT_EQ("https://bucket2.cos.region1.myqcloud.com/my%20key", task.uri());
  ASSERT_EQ(3, task.headers().size());
  EXPECT_EQ("x-cos-storage-class: MAZ_INTELLIGENT_TIERING", task.headers()[0]);
  EXPECT_EQ("x-cos-traffic-limit: 838860800", task.headers()[1]);
  EXPECT_EQ("x-cos-tagging: my tag", task.headers()[2]);
  EXPECT_EQ("bytes", flare::FlattenSlow(task.body()));
}

TEST(PutObject, result) {
  CosPutObjectResult result;
  ASSERT_TRUE(static_cast<cos::CosOperationResult*>(&result)->ParseResult(
      cos::CosTaskCompletion(HttpStatus::OK, HttpVersion::V_1_1,
                             {"x-cos-version-id: ver1"}, {}),
      {}));
  EXPECT_EQ("ver1", result.version_id);
}

}  // namespace flare
