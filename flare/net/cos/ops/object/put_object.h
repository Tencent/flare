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

#ifndef FLARE_NET_COS_OPS_OBJECT_PUT_OBJECT_H_
#define FLARE_NET_COS_OPS_OBJECT_PUT_OBJECT_H_

#include <string>

#include "flare/base/buffer.h"
#include "flare/net/cos/ops/operation.h"

namespace flare {

// This file implements COS's PutObject operation.
//
// @sa: https://cloud.tencent.com/document/product/436/7749 for documentation.

// Using `struct` instead of `class` for simplicity.

// PutObject request.
struct CosPutObjectRequest : cos::CosOperation {
  std::string key;

  // TODO(luobogao): RFC 2616 headers.
  // TODO(luobogao): `x-cos-meta-*`
  std::string storage_class;
  std::uint64_t traffic_limit = 0;
  std::string tagging;
  // TODO(luobogao): ACL.

  NoncontiguousBuffer bytes;

 private:
  bool PrepareTask(cos::CosTask* task, ErasedPtr* context) const override;
};

// PutObject response.
struct CosPutObjectResult : cos::CosOperationResult {
  std::string version_id;

 private:
  bool ParseResult(cos::CosTaskCompletion completion,
                   ErasedPtr context) override;
};

namespace cos {

// Maps request type to result type.
template <>
struct cos_result<CosPutObjectRequest> {
  using type = CosPutObjectResult;
};

}  // namespace cos

}  // namespace flare

#endif  // FLARE_NET_COS_OPS_OBJECT_PUT_OBJECT_H_
