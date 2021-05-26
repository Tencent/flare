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

#include "thirdparty/rapidxml/rapidxml.h"

#include "flare/base/encoding/percent.h"
#include "flare/net/cos/xml_reader.h"

namespace flare {

bool CosGetBucketRequest::PrepareTask(cos::CosTask* task,
                                      ErasedPtr* context) const {
  auto uri = Format(
      "https://{}.cos.{}.myqcloud.com/"
      "?prefix={}&delimiter={}&encoding-type={}&marker={}",
      task->options().bucket, task->options().region, EncodePercent(prefix),
      delimiter, "url", EncodePercent(marker));
  if (max_keys) {
    uri += Format("&max-keys={}", max_keys);
  }
  task->set_method(HttpMethod::Get);
  task->set_uri(uri);
  return true;
}

bool CosGetBucketResult::ParseResult(cos::CosTaskCompletion completion,
                                     ErasedPtr context) {
  auto resp = flare::FlattenSlow(*completion.body());
  try {
    rapidxml::xml_document doc;
    doc.parse<0>(resp.data());
    auto result = doc.first_node("ListBucketResult");
    if (!result) {
      FLARE_LOG_WARNING_EVERY_SECOND("Malformed response?");
      return false;
    }

    FLARE_COS_READ_XML_NODE_PCT_ENCODED(result, "Name", &name);
    FLARE_COS_READ_XML_NODE_PCT_ENCODED(result, "Prefix", &prefix);
    FLARE_COS_READ_XML_NODE_PCT_ENCODED(result, "Marker", &marker);
    FLARE_COS_READ_XML_NODE(result, "MaxKeys", &max_keys);
    FLARE_COS_READ_XML_NODE(result, "IsTruncated", &is_truncated);
    FLARE_COS_READ_XML_NODE_OPT(result, "Delimiter", &delimiter);
    FLARE_COS_READ_XML_NODE_PCT_ENCODED_OPT(result, "NextMarker", &next_marker);
    FLARE_COS_READ_XML_NODE_OPT(result, "EncodingType", &encoding_type);

    for (auto iter = result->first_node("Contents"); iter;
         iter = iter->next_sibling("Contents")) {
      auto&& entry = contents.emplace_back();

      FLARE_COS_READ_XML_NODE_PCT_ENCODED(iter, "Key", &entry.key);
      FLARE_COS_READ_XML_NODE(iter, "LastModified", &entry.last_modified);
      FLARE_COS_READ_XML_NODE(iter, "ETag", &entry.e_tag);
      FLARE_COS_READ_XML_NODE(iter, "Size", &entry.size);
    }
  } catch (const rapidxml::parse_error& err) {
    FLARE_LOG_WARNING_EVERY_SECOND("Failed to parse response: {}", err.what());
    return false;
  }
  return true;
}

}  // namespace flare
