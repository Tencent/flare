// Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef FLARE_FIBER_RUNTIME_H_
#define FLARE_FIBER_RUNTIME_H_

#include <cstdlib>

#include "flare/base/internal/annotation.h"
#include "flare/base/likely.h"

namespace flare::fiber {

namespace detail {

std::size_t GetCurrentSchedulingGroupIndexSlow();

}  // namespace detail

// Bring the whole world up.
//
// All stuffs about fiber are initialized by this method.
void StartRuntime();

// Bring the whole world down.
void TerminateRuntime();

// Get number of scheduling groups started.
std::size_t GetSchedulingGroupCount();

// Get the scheduling group the caller thread / fiber is currently belonging to.
//
// Calling this method outside of any scheduling group is undefined.
inline std::size_t GetCurrentSchedulingGroupIndex() {
  FLARE_INTERNAL_TLS_MODEL thread_local std::size_t index =
      detail::GetCurrentSchedulingGroupIndexSlow();
  return index;
}

// Get the scheduling group size.
std::size_t GetSchedulingGroupSize();

// Get NUMA node assigned to a given scheduling group. This method only makes
// sense if NUMA aware is enabled. Otherwise 0 is returned.
int GetSchedulingGroupAssignedNode(std::size_t sg_index);

namespace detail {

class SchedulingGroup;

// Find scheduling group by ID.
//
// Precondition: `index` < `GetSchedulingGroupCount().`
SchedulingGroup* GetSchedulingGroup(std::size_t index);

// Get scheduling group "nearest" to the calling thread.
//
// - If calling thread is a fiber worker, it's current scheduling group is
//   returned.
//
// - Otherwise if NUMA aware is enabled, a randomly chosen scheduling group in
//   the same node is returned.
//
// - If no scheduling group is initialized in current node, or NUMA aware is not
//   enabled, a randomly chosen one is returned.
//
// - If no scheduling group is initialize at all, `nullptr` is returned instead.
SchedulingGroup* NearestSchedulingGroupSlow(SchedulingGroup** cache);
inline SchedulingGroup* NearestSchedulingGroup() {
  FLARE_INTERNAL_TLS_MODEL thread_local SchedulingGroup* nearest{};
  if (FLARE_LIKELY(nearest)) {
    return nearest;
  }
  return NearestSchedulingGroupSlow(&nearest);
}

// Same as `NearestSchedulingGroup()`, but this one returns an index instead.
//
// Returns -1 if no scheduling group is initialized at all.
std::ptrdiff_t NearestSchedulingGroupIndex();

}  // namespace detail

}  // namespace flare::fiber

#endif  // FLARE_FIBER_RUNTIME_H_
