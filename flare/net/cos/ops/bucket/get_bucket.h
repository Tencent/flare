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

#ifndef FLARE_NET_COS_OPS_BUCKET_GET_BUCKET_H_
#define FLARE_NET_COS_OPS_BUCKET_GET_BUCKET_H_

#include <string>
#include <vector>

#include "flare/net/cos/ops/operation.h"

namespace flare {

// This file implements COS's GetBucket operation.
//
// @sa: https://cloud.tencent.com/document/product/436/7734 for documentation.

// Using `struct` instead of `class` for simplicity.

// GetBucket request.
struct CosGetBucketRequest : cos::CosOperation {
  std::string prefix;
  std::string delimiter;
  // Not sure if we should support `encoding-type`.
  std::string marker;
  std::uint64_t max_keys = 0;  // Using default limit.

 private:
  bool PrepareTask(cos::CosTask* task, ErasedPtr* context) const override;
};

// GetBucket response.
struct CosGetBucketResult : cos::CosOperationResult {
  std::string name;
  std::string encoding_type;
  std::string prefix;
  std::string marker;
  std::uint64_t max_keys;
  std::string delimiter;
  bool is_truncated = false;
  std::string next_marker;
  // Not sure how should we represent `CommonPrefixes`. Leave it out for now.

  struct Entry {
    std::string key;
    std::string last_modified;
    std::string e_tag;
    std::uint64_t size;
    // `Owner` / `StorageClass` / `StorageTier` is left our for now.
  };

  std::vector<Entry> contents;

 private:
  bool ParseResult(cos::CosTaskCompletion completion,
                   ErasedPtr context) override;
};

namespace cos {

// Maps request type to result type.
template <>
struct cos_result<CosGetBucketRequest> {
  using type = CosGetBucketResult;
};

}  // namespace cos

}  // namespace flare

#endif  // FLARE_NET_COS_OPS_BUCKET_GET_BUCKET_H_
