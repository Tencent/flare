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

#ifndef FLARE_FIBER_DETAIL_SCHEDULING_PARAMETERS_H_
#define FLARE_FIBER_DETAIL_SCHEDULING_PARAMETERS_H_

#include <cstddef>

// TODO(luobogao): Putting this file in the same directory as the fiber
// implementation looks weird. Maybe we should move fiber implementation into
// `fiber/internal`?

namespace flare::fiber::detail {

enum class SchedulingProfile {
  // Use this profile if your workload (running in fiber) tends to run long
  // (tens or hundres of milliseconds) without yielding fiber worker.
  //
  // For such use cases, it's important to share CPUs between fibers as much as
  // possible to avoid starvation, even at the cost of framework-internal
  // contention or sacrificing NUMA-locality.
  //
  // This profile:
  //
  // - Groups as many fiber workers as possible into a single work group.
  // - DISABLES NUMA awareness for fiber scheduling (but not object pool, etc.).
  ComputeHeavy,

  // Not as aggressive as `ComputeHeavy`. This profile prefers a large
  // scheduling group while still respect NUMA topology.
  //
  // This profile:
  //
  // - Enables NUMA awareness if requested concurrency is greater than half of
  //   available processors.
  // - So long as NUMA topology is respected, groups as many workers as possible
  //   into a single work group.
  Compute,

  // This profile tries to find a balance between reducing framework-internal
  // contention and encouraging sharing CPUs between fiber workers.
  //
  // This profile:
  //
  // - Uses a scheduling-group size between [16, 32).
  // - Enables NUMA awareness if (requested concurrency / number of NUMA nodes)
  //   results in a per-node-concurrency that fits in (or a multiple of) the
  //   scheduling-group size specification above.
  Neutral,

  // Use this profile if your workload tends to be quick, or yields a lot.
  //
  // For such use cases, we can use a smaller scheduling group to reduce
  // framework-internal contention, without risking starving fibers in run queue
  // for too long.
  //
  // This profile is the same as `Neutral` except that:
  //
  // - Uses a scheduling-group size between [12, 20).
  Io,

  // This profile prefers a smaller scheduling group, and is otherwise the same
  // as `Io`.
  //
  // This profile is the same as `Neutral` except that:
  //
  // - Uses a scheduling group size between [8, 16).
  IoHeavy,
};

struct SchedulingParameters {
  std::size_t scheduling_groups;
  std::size_t workers_per_group;

  // Possibly set only if scheduling groups can be distributed into NUMA domains
  // evenly.
  bool enable_numa_affinity;
};

// Determines scheduling parameters based on desired concurrency and profile.
SchedulingParameters GetSchedulingParameters(SchedulingProfile profile,
                                             std::size_t numa_domains,
                                             std::size_t available_processors,
                                             std::size_t desired_concurrency);

}  // namespace flare::fiber::detail

#endif  // FLARE_FIBER_DETAIL_SCHEDULING_PARAMETERS_H_
