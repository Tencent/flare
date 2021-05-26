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

#ifndef FLARE_NET_COS_OPS_OBJECT_DELETE_OBJECT_H_
#define FLARE_NET_COS_OPS_OBJECT_DELETE_OBJECT_H_

#include <string>

#include "flare/base/buffer.h"
#include "flare/net/cos/ops/operation.h"

namespace flare {

// This file implements COS's DeleteObject operation.
//
// @sa: https://cloud.tencent.com/document/product/436/7743 for documentation.

// Using `struct` instead of `class` for simplicity.

// DeleteObject request.
struct CosDeleteObjectRequest : cos::CosOperation {
  std::string key;
  std::string version_id;

 private:
  bool PrepareTask(cos::CosTask* task, ErasedPtr* context) const override;
};

// DeleteObject response.
struct CosDeleteObjectResult : cos::CosOperationResult {
  std::string version_id;
  bool delete_marker = false;

 private:
  bool ParseResult(cos::CosTaskCompletion completion,
                   ErasedPtr context) override;
};

namespace cos {

// Maps request type to result type.
template <>
struct cos_result<CosDeleteObjectRequest> {
  using type = CosDeleteObjectResult;
};

}  // namespace cos

}  // namespace flare

#endif  // FLARE_NET_COS_OPS_OBJECT_DELETE_OBJECT_H_
