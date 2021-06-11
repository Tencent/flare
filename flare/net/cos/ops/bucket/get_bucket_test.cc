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

#include "flare/net/cos/ops/bucket/get_bucket.h"

#include "googletest/gtest/gtest.h"

namespace flare {

TEST(GetBucket, Request) {
  CosGetBucketRequest req;
  req.max_keys = 12345;

  cos::CosTask::Options opts = {.region = "region1", .bucket = "bucket2"};
  cos::CosTask task(&opts);
  // Using `FRIEND_TEST` should do the trick, as well.
  ASSERT_TRUE(
      static_cast<cos::CosOperation*>(&req)->PrepareTask(&task, nullptr));

  EXPECT_EQ(HttpMethod::Get, task.method());
  EXPECT_EQ(
      "https://bucket2.cos.region1.myqcloud.com/"
      "?prefix=&delimiter=&encoding-type=url&marker=&max-keys=12345",
      task.uri());
}

TEST(GetBucket, result) {
  constexpr auto body =
      "  <?xml version='1.0' encoding='utf-8' ?>\n"
      "<ListBucketResult>\n"
      "    <Name>examplebucket-1250000000</Name>\n"
      "    <Prefix/>\n"
      "    <Marker/>\n"
      "    <MaxKeys>1000</MaxKeys>\n"
      "    <IsTruncated>false</IsTruncated>\n"
      "    <Contents>\n"
      "        <Key>example-folder-1/example-object-1.jpg</Key>\n"
      "        <LastModified>2020-12-10T03:37:30.000Z</LastModified>\n"
      "        <ETag>&quot;f173c1199e3d3b53dd91223cae16fb42&quot;</ETag>\n"
      "        <Size>37</Size>\n"
      "        <Owner>\n"
      "            <ID>1250000000</ID>\n"
      "            <DisplayName>1250000000</DisplayName>\n"
      "        </Owner>\n"
      "        <StorageClass>STANDARD</StorageClass>\n"
      "    </Contents>\n"
      "    <Contents>\n"
      "        <Key>example-folder-1/example-object-2.jpg</Key>\n"
      "        <LastModified>2020-12-10T03:37:30.000Z</LastModified>\n"
      "        <ETag>&quot;c9d28698978bb6fef6c1ed1c439a17d3&quot;</ETag>\n"
      "        <Size>37</Size>\n"
      "        <Owner>\n"
      "            <ID>1250000000</ID>\n"
      "            <DisplayName>1250000000</DisplayName>\n"
      "        </Owner>\n"
      "        <StorageClass>INTELLIGENT_TIERING</StorageClass>\n"
      "        <StorageTier>FREQUENT</StorageTier>\n"
      "    </Contents>\n"
      "    <Contents>\n"
      "        <Key>example-object-2.jpg</Key>\n"
      "        <LastModified>2020-12-10T03:37:30.000Z</LastModified>\n"
      "        <ETag>&quot;51370fc64b79d0d3c7c609635be1c41f&quot;</ETag>\n"
      "        <Size>20</Size>\n"
      "        <Owner>\n"
      "            <ID>1250000000</ID>\n"
      "            <DisplayName>1250000000</DisplayName>\n"
      "        </Owner>\n"
      "        <StorageClass>STANDARD_IA</StorageClass>\n"
      "    </Contents>\n"
      "</ListBucketResult>\n";

  CosGetBucketResult result;
  ASSERT_TRUE(static_cast<cos::CosOperationResult*>(&result)->ParseResult(
      cos::CosTaskCompletion(HttpStatus::OK, HttpVersion::V_1_1, {},
                             flare::CreateBufferSlow(body)),
      {}));
  ASSERT_EQ(3, result.contents.size());
  EXPECT_EQ("examplebucket-1250000000", result.name);
  EXPECT_EQ("example-object-2.jpg", result.contents[2].key);
  EXPECT_EQ("\"51370fc64b79d0d3c7c609635be1c41f\"", result.contents[2].e_tag);
}

}  // namespace flare
