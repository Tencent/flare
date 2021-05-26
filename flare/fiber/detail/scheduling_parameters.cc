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

#include "flare/fiber/detail/scheduling_parameters.h"

#include "flare/base/enum.h"
#include "flare/base/internal/cpu.h"
#include "flare/base/logging.h"

namespace flare::fiber::detail {

constexpr auto kMaximumSchedulingGroupSize = 64;

SchedulingParameters GetSchedulingParametersForComputeHeavy(
    std::size_t concurrency) {
  auto groups = (concurrency + kMaximumSchedulingGroupSize - 1) /
                kMaximumSchedulingGroupSize;
  auto group_size = (concurrency + groups - 1) / groups;
  return SchedulingParameters{.scheduling_groups = groups,
                              .workers_per_group = group_size,
                              .enable_numa_affinity = false};
}

SchedulingParameters GetSchedulingParametersForCompute(
    std::size_t numa_domains, std::size_t available_processors,
    std::size_t desired_concurrency) {
  auto numa_aware =
      numa_domains > 1 && desired_concurrency * 2 >= available_processors;
  if (!numa_aware) {
    return GetSchedulingParametersForComputeHeavy(desired_concurrency);
  }
  auto per_node = (desired_concurrency + numa_domains - 1) / numa_domains;
  auto group_per_node = (per_node + kMaximumSchedulingGroupSize - 1) /
                        kMaximumSchedulingGroupSize;
  auto group_size = (per_node + group_per_node - 1) / group_per_node;
  return SchedulingParameters{
      .scheduling_groups = group_per_node * numa_domains,
      .workers_per_group = group_size,
      .enable_numa_affinity = true};
}

SchedulingParameters GetSchedulingParametersOfGroupSize(
    std::size_t numa_domains, std::size_t concurrency,
    std::size_t group_size_low, std::size_t group_size_high) {
  if (concurrency <= group_size_low) {
    return SchedulingParameters{.scheduling_groups = 1,
                                .workers_per_group = concurrency,
                                .enable_numa_affinity = false};
  }

  bool numa_aware = true;
  std::size_t best_group_size_so_far = 0;
  std::size_t best_group_extra_workers = 0x7fff'ffff;

  // Try respect NUMA topology first.
  if (numa_domains > 1) {
    for (int i = group_size_low; i != group_size_high; ++i) {
      auto groups = (concurrency + i - 1) / i;
      auto extra = groups * i - concurrency;
      if (groups % numa_domains != 0) {
        continue;
      }
      if (extra < best_group_extra_workers) {
        best_group_extra_workers = extra;
        best_group_size_so_far = i;
      }
    }
  }

  // If we can't find a suitable configuration w.r.t NUMA topology, retry with
  // UMA configuration.
  if (!best_group_size_so_far || best_group_extra_workers > concurrency / 10) {
    numa_aware = false;
    best_group_extra_workers = 0x7fff'ffff;
    for (int i = group_size_low; i != group_size_high; ++i) {
      auto groups = (concurrency + i - 1) / i;
      auto extra = groups * i - concurrency;
      if (extra < best_group_extra_workers) {
        best_group_extra_workers = extra;
        best_group_size_so_far = i;
      }
    }
  }

  return SchedulingParameters{
      .scheduling_groups =
          (concurrency + best_group_size_so_far - 1) / best_group_size_so_far,
      .workers_per_group = best_group_size_so_far,
      .enable_numa_affinity = numa_aware};
}

SchedulingParameters GetSchedulingParameters(SchedulingProfile profile,
                                             std::size_t numa_domains,
                                             std::size_t available_processors,
                                             std::size_t desired_concurrency) {
  if (profile == SchedulingProfile::ComputeHeavy) {
    return GetSchedulingParametersForComputeHeavy(desired_concurrency);
  } else if (profile == SchedulingProfile::Compute) {
    return GetSchedulingParametersForCompute(numa_domains, available_processors,
                                             desired_concurrency);
  } else if (profile == SchedulingProfile::Neutral) {
    // @sa: `SchedulingProfile` for the constants below.
    return GetSchedulingParametersOfGroupSize(numa_domains, desired_concurrency,
                                              16, 32);
  } else if (profile == SchedulingProfile::Io) {
    return GetSchedulingParametersOfGroupSize(numa_domains, desired_concurrency,
                                              12, 24);
  } else if (profile == SchedulingProfile::IoHeavy) {
    return GetSchedulingParametersOfGroupSize(numa_domains, desired_concurrency,
                                              8, 16);
  }
  FLARE_UNREACHABLE("Unexpected scheduling profile [{}].",
                    underlying_value(profile));
}

}  // namespace flare::fiber::detail
