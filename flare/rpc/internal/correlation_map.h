// Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef FLARE_RPC_INTERNAL_CORRELATION_MAP_H_
#define FLARE_RPC_INTERNAL_CORRELATION_MAP_H_

#include <vector>

#include "flare/base/id_alloc.h"
#include "flare/base/never_destroyed.h"
#include "flare/fiber/runtime.h"
#include "flare/rpc/internal/sharded_call_map.h"

// Here we use a semi-global correlation map for all outgoing RPCs.
//
// This helps us to separate timeout logic from normal RPCs. Now we don't need
// correpsonding connection object to be alive to deal with timeout timers.
//
// This is not the case previously. As the timeout timer may access the
// correlation map in corresponding connection object, we must either keep the
// connection alive, or wait for timer to be quiescent before destroying the
// connection. Either way, we'll introduce a synchronization overhead.
//
// By moving timeout out from connection handling, it also brings us another
// benefit: Now we can handle timeout and backup-request in a similar fashion.
// Were timeout detected in connection objects, implementing backup-request
// would require both connection and channel objects to keep a timer, one for
// detecting timeout, and another for firing backup-request. That can be messy.

namespace flare::rpc::internal {

// TODO(luobogao): Now that we're using a (semi-)global map, we can use a
// fixed-sized lockless map instead of `ShardedCallMap` for more stable
// performance.
//
// The reason why we can't do this previously is that we can't afford to keep a
// large fixed-size map for each connection. Now that it's semi-global, it's
// affordable.
//
// TODO(luobogao): Expose statistics of each correlation map via `ExposedVar`.
template <class T>
using CorrelationMap = ShardedCallMap<T>;

// Get correlation map for the given scheduling group. The resulting map is
// indexed by key generated via `MergeCorrelationId`.
template <class T>
CorrelationMap<T>* GetCorrelationMapFor(std::size_t scheduling_group_id) {
  static NeverDestroyed<std::vector<CorrelationMap<T>>> maps(
      fiber::GetSchedulingGroupCount());
  FLARE_CHECK_LT(scheduling_group_id, maps->size());
  return &(*maps)[scheduling_group_id];
}

}  // namespace flare::rpc::internal

#endif  // FLARE_RPC_INTERNAL_CORRELATION_MAP_H_
