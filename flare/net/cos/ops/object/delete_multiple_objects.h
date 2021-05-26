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

#ifndef FLARE_NET_COS_OPS_OBJECT_DELETE_MULTIPLE_OBJECTS_H_
#define FLARE_NET_COS_OPS_OBJECT_DELETE_MULTIPLE_OBJECTS_H_

#include <string>
#include <vector>

#include "flare/base/buffer.h"
#include "flare/net/cos/ops/operation.h"

namespace flare {

// This file implements COS's DeleteMultipleObjects operation.
//
// @sa: https://cloud.tencent.com/document/product/436/8289 for documentation.

// Using `struct` instead of `class` for simplicity.

// DeleteMultipleObjects request.
struct CosDeleteMultipleObjectsRequest : cos::CosOperation {
  bool quiet = false;

  struct Entry {
    std::string key;
    std::string version_id;
  };
  std::vector<Entry> objects;

 private:
  bool PrepareTask(cos::CosTask* task, ErasedPtr* context) const override;
};

// DeleteMultipleObjects response.
struct CosDeleteMultipleObjectsResult : cos::CosOperationResult {
  struct Deleted {
    std::string key;
    bool delete_marker;
    std::string delete_marker_version_id;
    std::string version_id;
  };

  // This object should be recognizable to `ParseCosStatus`. Not sure if we want
  // to convert it for the user.
  struct Error {
    std::string key;
    std::string version_id;
    std::string code;
    std::string message;
  };

  std::vector<Deleted> deleted;
  std::vector<Error> error;

 private:
  bool ParseResult(cos::CosTaskCompletion completion,
                   ErasedPtr context) override;
};

namespace cos {

// Maps request type to result type.
template <>
struct cos_result<CosDeleteMultipleObjectsRequest> {
  using type = CosDeleteMultipleObjectsResult;
};

}  // namespace cos

}  // namespace flare

#endif  // FLARE_NET_COS_OPS_OBJECT_DELETE_MULTIPLE_OBJECTS_H_
