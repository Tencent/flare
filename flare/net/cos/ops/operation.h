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

#ifndef FLARE_NET_COS_OPS_OPERATION_H_
#define FLARE_NET_COS_OPS_OPERATION_H_

#include "flare/base/erased_ptr.h"
#include "flare/net/cos/ops/task.h"

namespace flare::cos {

// This interface interacts with `CosClient` to issue COS requests.
//
// Not sure if the member fields should be `public`..
class CosOperation {
 public:
  virtual ~CosOperation() = default;

  // Build HTTP request representing this task.
  //
  // `context`, if filled, is passed as-is to `ParseResult` (see below).
  virtual bool PrepareTask(CosTask* task, ErasedPtr* context) const = 0;
};

// `CosClient` uses this interface to parse response from COS server.
//
// Not sure if the member fields should be `public`..
class CosOperationResult {
 public:
  virtual ~CosOperationResult() = default;

  // Called upon HTTP response arrival. This method is only called if the
  // request was completed successfully in time.
  virtual bool ParseResult(CosTaskCompletion completion, ErasedPtr context) = 0;
};

// All COS operation class should specialize this class template. `CosClient`
// needs it to define its interface.
template <class T>
struct cos_result {};

template <class T>
using cos_result_t = typename cos_result<T>::type;

}  // namespace flare::cos

#endif  // FLARE_NET_COS_OPS_OPERATION_H_
