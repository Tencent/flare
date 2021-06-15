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

#include "flare/base/internal/background_task_host.h"

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "gflags/gflags.h"

#include "flare/base/internal/cpu.h"
#include "flare/base/internal/logging.h"
#include "flare/base/random.h"
#include "flare/base/thread/attribute.h"

DEFINE_int32(flare_background_task_host_workers_per_node, 4,
             "Number of pthread workers for running background tasks per NUMA "
             "node. The default value should work well in most cases.");
DEFINE_int32(flare_background_task_host_workers_nice_value, 5,
             "Determines niceness of background host workers.");
DEFINE_string(
    flare_background_task_host_workers_affinity, "",
    "If set, determines affinity of background worker threads. It is suggested "
    "that you assign processors in each NUMA domain equally to background task "
    "workers, as they'll only use processors in their own NUMA domain (unless "
    "no processor in that domain is assigned). Processors can be specified "
    "either as individual IDs or ranges, e.g., 1-3,6.");

namespace flare::internal {

namespace {

std::set<int> GetAccessibleProcessors() {
  if (FLAGS_flare_background_task_host_workers_affinity.empty()) {
    auto affinity = GetCurrentThreadAffinity();
    return std::set<int>(affinity.begin(), affinity.end());
  }
  auto opt =
      TryParseProcesserList(FLAGS_flare_background_task_host_workers_affinity);
  FLARE_CHECK(opt,
              "Failed to parse `flare_background_task_host_workers_affinity`.");
  return std::set<int>(opt->begin(), opt->end());
}

}  // namespace

BackgroundTaskHost* BackgroundTaskHost::Instance() {
  static NeverDestroyedSingleton<BackgroundTaskHost> bth;
  return bth.Get();
}

void BackgroundTaskHost::Start() {
  auto accessible = GetAccessibleProcessors();
  auto workers_per_node = FLAGS_flare_background_task_host_workers_per_node;

  auto topo = numa::GetAvailableNodes();
  FLARE_CHECK(topo.size() == numa::GetNumberOfNodesAvailable());
  pools_.resize(topo.size());
  if (topo.size() == 1) {
    // UMA, easy case.
    pools_[0] = std::make_unique<ThreadPool>(
        workers_per_node,
        std::vector<int>(accessible.begin(), accessible.end()),
        FLAGS_flare_background_task_host_workers_nice_value);
  } else {
    for (int i = 0; i != topo.size(); ++i) {
      FLARE_CHECK(!pools_[numa::GetNodeIndex(topo[i].id)],
                  "Duplicate NUMA ID found?");
      auto procs = topo[i].logical_cpus;
      procs.erase(
          std::remove_if(procs.begin(), procs.end(),
                         [&](auto&& e) { return accessible.count(e) == 0; }),
          procs.end());
      if (procs.empty()) {
        FLARE_LOG_WARNING(
            "Background task host in NUMA domain #{} is not assigned any "
            "processors, using processors in other domains.",
            topo[i].id);
        procs = std::vector<int>(accessible.begin(), accessible.end());
      }
      pools_[i] = std::make_unique<ThreadPool>(
          workers_per_node, procs,
          FLAGS_flare_background_task_host_workers_nice_value);
    }
  }
}

void BackgroundTaskHost::Stop() {
  for (auto&& e : pools_) {
    e->Stop();
  }
}

void BackgroundTaskHost::Join() {
  for (auto&& e : pools_) {
    e->Join();
  }
  pools_.clear();
}

void BackgroundTaskHost::Queue(Function<void()>&& op) {
  pools_[Random() % pools_.size()]->QueueJob(std::move(op));
}

void BackgroundTaskHost::Queue(std::uint64_t numa_id, Function<void()>&& op) {
  auto index = numa::GetNodeIndex(numa_id);
  FLARE_CHECK_LT(index, pools_.size());
  FLARE_CHECK(!!pools_[index], "NUMA node #{} was not accessible upon startup?",
              numa_id);
  pools_[index]->QueueJob(std::move(op));
}

BackgroundTaskHost::BackgroundTaskHost() = default;

}  // namespace flare::internal
