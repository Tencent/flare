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

#include "flare/base/encoding/percent.h"

namespace flare {

bool CosGetObjectRequest::PrepareTask(cos::CosTask* task,
                                      ErasedPtr* context) const {
  task->set_method(HttpMethod::Get);

  auto uri =
      Format("https://{}.cos.{}.myqcloud.com/{}?", task->options().bucket,
             task->options().region, EncodePercent(key));
  if (!version_id.empty()) {
    uri += Format("versionId={}&", version_id);
  }
  task->set_uri(uri);

  if (traffic_limit) {
    task->AddHeader(Format("x-cos-traffic-limit: {}", traffic_limit));
  }
  return true;
}

bool CosGetObjectResult::ParseResult(cos::CosTaskCompletion completion,
                                     ErasedPtr context) {
  auto&& hdrs = *completion.headers();
  storage_class = hdrs.TryGet("x-cos-storage-class").value_or("");
  storage_tier = hdrs.TryGet("x-cos-storage-tier").value_or("");
  version_id = hdrs.TryGet("x-cos-version-id").value_or("");
  bytes = std::move(*completion.body());
  return true;
}

}  // namespace flare
