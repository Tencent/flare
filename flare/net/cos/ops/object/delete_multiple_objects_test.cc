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

#include "flare/net/cos/ops/object/delete_multiple_objects.h"

#include <string_view>

#include "googletest/gtest/gtest.h"
#include "rapidxml/rapidxml.h"

using namespace std::literals;

namespace flare {

TEST(DeleteMultipleObjects, Request) {
  CosDeleteMultipleObjectsRequest req;
  req.quiet = true;
  req.objects.emplace_back() = {"key", "version_id"};
  req.objects.emplace_back() = {"key2"};
  cos::CosTask::Options opts = {.region = "region1", .bucket = "bucket2"};
  cos::CosTask task(&opts);
  ASSERT_TRUE(
      static_cast<cos::CosOperation*>(&req)->PrepareTask(&task, nullptr));

  EXPECT_EQ(HttpMethod::Post, task.method());
  EXPECT_EQ("https://bucket2.cos.region1.myqcloud.com/?delete", task.uri());

  auto xml_str = flare::FlattenSlow(task.body());
  std::cout << xml_str << std::endl;
  rapidxml::xml_document doc;
  doc.parse<0>(const_cast<char*>(xml_str.c_str()));
  auto&& req_xml = doc.first_node("Delete");
  EXPECT_EQ("true"sv, req_xml->first_node("Quiet")->value());

  auto iter = req_xml->first_node("Object");
  EXPECT_EQ("key"sv, iter->first_node("Key")->value());
  EXPECT_EQ("version_id"sv, iter->first_node("VersionId")->value());

  iter = iter->next_sibling();
  EXPECT_EQ("key2"sv, iter->first_node("Key")->value());
  EXPECT_EQ(nullptr, iter->first_node("VersionId"));
}

TEST(DeleteMultipleObjects, result) {
  auto xml =
      "<DeleteResult>"
      "    <Deleted>"
      "        <Key>example-object-1.jpg</Key>"
      "        <DeleteMarker>true</DeleteMarker>"
      "        <DeleteMarkerVersionId>MTg0NDUxNzc2ODQ2NjM1NTI2NDY</"
      "DeleteMarkerVersionId>"
      "    </Deleted>"
      "    <Deleted>"
      "        <Key>example-object-2.jpg</Key>"
      "        <VersionId>MTg0NDUxNzc2ODQ2NjQ1MjM5MTk</VersionId>"
      "    </Deleted>"
      "    <Deleted>"
      "        <Key>example-object-3.jpg</Key>"
      "        <DeleteMarker>true</DeleteMarker>"
      "        <DeleteMarkerVersionId>MTg0NDUxNzc2ODQ2NjQwMTIwMDI</"
      "DeleteMarkerVersionId>"
      "        <VersionId>MTg0NDUxNzc2ODQ2NjQwMTIwMDI</VersionId>"
      "    </Deleted>"
      "    <Error>"
      "        <Key>example-object-4.jpg</Key>"
      "        <VersionId>MTg0NDUxNzc2ODQ2NjQ0NjI0MDQ</VersionId>"
      "        <Code>PathConflict</Code>"
      "        <Message>Path conflict.</Message>"
      "    </Error>"
      "</DeleteResult>";
  CosDeleteMultipleObjectsResult result;
  ASSERT_TRUE(static_cast<cos::CosOperationResult*>(&result)->ParseResult(
      cos::CosTaskCompletion(HttpStatus::OK, HttpVersion::V_1_1, {},
                             flare::CreateBufferSlow(xml)),
      {}));
  ASSERT_EQ(3, result.deleted.size());

  ASSERT_EQ(1, result.error.size());
  EXPECT_EQ("example-object-1.jpg", result.deleted[0].key);
  EXPECT_TRUE(result.deleted[0].delete_marker);
  EXPECT_EQ("MTg0NDUxNzc2ODQ2NjM1NTI2NDY",
            result.deleted[0].delete_marker_version_id);

  EXPECT_EQ("example-object-2.jpg", result.deleted[1].key);
  EXPECT_EQ("MTg0NDUxNzc2ODQ2NjQ1MjM5MTk", result.deleted[1].version_id);

  EXPECT_EQ("example-object-3.jpg", result.deleted[2].key);
  EXPECT_TRUE(result.deleted[2].delete_marker);
  EXPECT_EQ("MTg0NDUxNzc2ODQ2NjQwMTIwMDI",
            result.deleted[2].delete_marker_version_id);
  EXPECT_EQ("MTg0NDUxNzc2ODQ2NjQwMTIwMDI", result.deleted[2].version_id);

  EXPECT_EQ("example-object-4.jpg", result.error[0].key);
  EXPECT_EQ("MTg0NDUxNzc2ODQ2NjQ0NjI0MDQ", result.error[0].version_id);
  EXPECT_EQ("PathConflict", result.error[0].code);
  EXPECT_EQ("Path conflict.", result.error[0].message);
}

}  // namespace flare
