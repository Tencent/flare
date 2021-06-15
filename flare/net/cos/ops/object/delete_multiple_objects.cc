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

#include <sstream>

#include "rapidxml/rapidxml.h"

#include "flare/base/encoding/percent.h"
#include "flare/net/cos/xml_reader.h"

namespace flare {

bool CosDeleteMultipleObjectsRequest::PrepareTask(cos::CosTask* task,
                                                  ErasedPtr* context) const {
  rapidxml::xml_document xml_req;
  auto xml_root = xml_req.allocate_node(rapidxml::node_element, "Delete");
  xml_req.append_node(xml_root);
  xml_root->append_node(xml_req.allocate_node(rapidxml::node_element, "Quiet",
                                              quiet ? "true" : "false"));
  for (auto&& e : objects) {
    auto xml_node = xml_req.allocate_node(rapidxml::node_element, "Object");
    xml_node->append_node(
        xml_req.allocate_node(rapidxml::node_element, "Key", e.key.c_str()));
    if (!e.version_id.empty()) {
      xml_node->append_node(xml_req.allocate_node(
          rapidxml::node_element, "VersionId", e.version_id.c_str()));
    }
    xml_root->append_node(xml_node);
  }
  std::stringstream ss;
  ss << xml_req;

  auto uri = Format("https://{}.cos.{}.myqcloud.com/?delete",
                    task->options().bucket, task->options().region);

  task->set_method(HttpMethod::Post);
  task->set_uri(uri);
  task->AddHeader("Content-Type: application/xml");
  task->set_body(flare::CreateBufferSlow(ss.str()));
  return true;
}

bool CosDeleteMultipleObjectsResult::ParseResult(
    cos::CosTaskCompletion completion, ErasedPtr context) {
  auto resp = flare::FlattenSlow(*completion.body());
  try {
    rapidxml::xml_document doc;
    doc.parse<0>(resp.data());
    auto result = doc.first_node("DeleteResult");
    if (!result) {
      FLARE_LOG_WARNING_EVERY_SECOND("Malformed response?");
      return false;
    }

    for (auto iter = result->first_node("Deleted"); iter;
         iter = iter->next_sibling("Deleted")) {
      auto&& entry = deleted.emplace_back();
      FLARE_COS_READ_XML_NODE_PCT_ENCODED(iter, "Key", &entry.key);
      FLARE_COS_READ_XML_NODE_OPT(iter, "DeleteMarker", &entry.delete_marker);
      FLARE_COS_READ_XML_NODE_OPT(iter, "DeleteMarkerVersionId",
                                  &entry.delete_marker_version_id);
      FLARE_COS_READ_XML_NODE_OPT(iter, "VersionId", &entry.version_id);
    }

    for (auto iter = result->first_node("Error"); iter;
         iter = iter->next_sibling("Error")) {
      auto&& entry = error.emplace_back();
      FLARE_COS_READ_XML_NODE_PCT_ENCODED(iter, "Key", &entry.key);
      FLARE_COS_READ_XML_NODE_OPT(iter, "VersionId", &entry.version_id);
      FLARE_COS_READ_XML_NODE(iter, "Code", &entry.code);
      FLARE_COS_READ_XML_NODE(iter, "Message", &entry.message);
    }
  } catch (const rapidxml::parse_error& err) {
    FLARE_LOG_WARNING_EVERY_SECOND("Failed to parse response: {}", err.what());
    return false;
  }
  return true;
}

}  // namespace flare
