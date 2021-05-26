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

#ifndef FLARE_NET_COS_CHANNEL_H_
#define FLARE_NET_COS_CHANNEL_H_

#include "flare/base/expected.h"
#include "flare/base/future.h"
#include "flare/base/status.h"
#include "flare/net/cos/ops/operation.h"

namespace flare::cos {

// This interface eases implementing several testing facilities.
class Channel {
 public:
  virtual ~Channel() = default;

  // Perform `op` and store result in `result`. `done` is called on completion.
  virtual void Perform(const cos::Channel*, const CosOperation& op,
                       CosOperationResult* result,
                       const CosTask::Options& options,
                       std::chrono::nanoseconds timeout,
                       Function<void(Status)>* done) = 0;
};

}  // namespace flare::cos

#endif  // FLARE_NET_COS_CHANNEL_H_
