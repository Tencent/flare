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

#include "flare/fiber/runtime.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "thirdparty/gflags/gflags.h"

#include "flare/base/internal/annotation.h"
#include "flare/base/internal/cpu.h"
#include "flare/base/random.h"
#include "flare/base/string.h"
#include "flare/base/thread/attribute.h"
#include "flare/fiber/detail/fiber_worker.h"
#include "flare/fiber/detail/scheduling_group.h"
#include "flare/fiber/detail/scheduling_parameters.h"
#include "flare/fiber/detail/timer_worker.h"

DEFINE_int32(
    flare_concurrency_hint, 0,
    "Hint to how many threads should be started to run fibers. Flare may "
    "adjust this value when it deems fit. The default is `nproc()`.");
DEFINE_int32(flare_scheduling_group_size, 0,
             "Internally Flare divides worker threads into groups, and tries "
             "to avoid sharing between them. This option controls group size "
             "of workers. Setting it too small may result in unbalanced "
             "workload, setting it too large can hurt overall scalability.");
DEFINE_bool(flare_numa_aware, true,
            "If set, Flare allocates (binds) worker threads (in group) to CPU "
            "nodes. Otherwise it's up to OS's scheduler to determine which "
            "worker thread should run on which CPU (/node).");
DEFINE_string(flare_fiber_worker_accessible_cpus, "",
              "If set, fiber workers only use CPUs given. CPUs are specified "
              "in range or CPU IDs, e.g.: 0-10,11,12. Negative CPU IDs can be "
              "used to specify CPU IDs in opposite order (e.g., -1 means the "
              "last CPU.). Negative IDs can only be specified individually due "
              "to difficulty in parse. This option may not be used in "
              "conjuction with `flare_fiber_worker_inaccessible_cpus`.");
DEFINE_string(
    flare_fiber_worker_inaccessible_cpus, "",
    "If set, fiber workers use CPUs that are NOT listed here. Both CPU ID "
    "ranges or individual IDs are recognized. This option may not be used in "
    "conjuction with `flare_fiber_worker_accessible_cpus`. CPUs that are not "
    "accessible to us due to thread affinity or other resource contraints are "
    "also respected when this option is used, you don't have to (yet, not "
    "prohibited to) specify them in the list. ");
DEFINE_bool(
    flare_fiber_worker_disallow_cpu_migration, false,
    "If set, each fiber worker is bound to exactly one CPU core, and each core "
    "is dedicated to exactly one fiber worker. `flare_concurrency_hint` (if "
    "set) must be equal to the number of CPUs in the system (or in case "
    "`flare_fiber_worker_accessible_cpus` is set as well, the number of CPUs "
    "accessible to fiber worker.). Incorrect use of this option can actually "
    "hurt performance.");
DEFINE_int32(flare_work_stealing_ratio, 16,
             "Reciprocal of ratio for stealing job from other scheduling "
             "groups in same NUMA domain. Note that if multiple \"foreign\" "
             "scheduling groups present, the actual work stealing ratio is "
             "multiplied by foreign scheduling group count.");
DEFINE_string(
    flare_fiber_scheduling_optimize_for, "neutral",
    "This option controls for which use case should fiber scheduling parameter "
    "optimized for. The valid choices are 'compute-heavy', 'compute', "
    "'neutral', 'io', 'io-heavy', 'customized'. Optimize for computation if "
    "your workloads tend to run long (without triggering fiber scheduling), "
    "optimize for I/O if your workloads run quickly or triggers fiber "
    "scheduling often. If none of the predefine optimization profile suits "
    "your needs, use `customized` and specify "
    "`scheduling_parameters.workers_per_group` "
    "and `flare_numa_aware` with your own choice.");

// In our test, cross-NUMA work stealing hurts performance.
//
// The performance hurt comes from multiple aspects, notably the imbalance in
// shared pool of `MemoryNodeShared` object pool.
//
// Therefore, by default, we disables cross-NUMA work stealing completely.
DEFINE_int32(flare_cross_numa_work_stealing_ratio, 0,
             "Same as `flare_work_stealing_ratio`, but applied to stealing "
             "work from scheduling groups belonging to different NUMA domain. "
             "Setting it to 0 disables stealing job cross NUMA domain. Blindly "
             "enabling this options can actually hurt performance. You should "
             "do thorough test before changing this option.");

namespace flare::fiber {

namespace {

// `SchedulingGroup` and its workers (both fiber worker and timer worker).
struct FullyFledgedSchedulingGroup {
  int node_id;
  std::unique_ptr<detail::SchedulingGroup> scheduling_group;
  std::vector<std::unique_ptr<detail::FiberWorker>> fiber_workers;
  std::unique_ptr<detail::TimerWorker> timer_worker;

  void Start(bool no_cpu_migration) {
    timer_worker->Start();
    for (auto&& e : fiber_workers) {
      e->Start(no_cpu_migration);
    }
  }

  void Stop() {
    timer_worker->Stop();
    scheduling_group->Stop();
  }

  void Join() {
    timer_worker->Join();
    for (auto&& e : fiber_workers) {
      e->Join();
    }
  }
};

// Final decision of scheduling parameters.
std::size_t fiber_concurrency_in_effect = 0;
detail::SchedulingParameters scheduling_parameters;

// Index by node ID. i.e., `scheduling_group[node][sg_index]`
//
// If `flare_numa_aware` is not set, `node` should always be 0.
//
// 64 nodes should be enough.
std::vector<std::unique_ptr<FullyFledgedSchedulingGroup>> scheduling_groups[64];

// This vector holds pointer to scheduling groups in `scheduling_groups`. It's
// primarily used for randomly choosing a scheduling group or finding scheduling
// group by ID.
std::vector<FullyFledgedSchedulingGroup*> flatten_scheduling_groups;

const std::vector<int>& GetFiberWorkerAccessibleCPUs();
const std::vector<internal::numa::Node>& GetFiberWorkerAccessibleNodes();

std::uint64_t DivideRoundUp(std::uint64_t divisor, std::uint64_t dividend) {
  return divisor / dividend + (divisor % dividend != 0);
}

// Call `f` in a thread with the specified affinity.
//
// This method helps you allocates resources from memory attached to one of the
// CPUs listed in `affinity`, instead of the calling node (unless they're the
// same).
template <class F>
void ExecuteWithAffinity(const std::vector<int>& affinity, F&& f) {
  // Dirty but works.
  //
  // TODO(luobogao): Set & restore this thread's affinity to `affinity` (instead
  // of starting a new thread) to accomplish this.
  std::thread([&] {
    SetCurrentThreadAffinity(affinity);
    std::forward<F>(f)();
  }).join();
}

std::unique_ptr<FullyFledgedSchedulingGroup> CreateFullyFledgedSchedulingGroup(
    int node_id, const std::vector<int>& affinity, std::size_t size) {
  FLARE_CHECK(!FLAGS_flare_fiber_worker_disallow_cpu_migration ||
              affinity.size() == size);
  // TODO(luobogao): Create these objects in a thread with affinity `affinity.
  auto rc = std::make_unique<FullyFledgedSchedulingGroup>();

  rc->node_id = node_id;
  rc->scheduling_group =
      std::make_unique<detail::SchedulingGroup>(affinity, size);
  for (int i = 0; i != size; ++i) {
    rc->fiber_workers.push_back(
        std::make_unique<detail::FiberWorker>(rc->scheduling_group.get(), i));
  }
  rc->timer_worker =
      std::make_unique<detail::TimerWorker>(rc->scheduling_group.get());
  rc->scheduling_group->SetTimerWorker(rc->timer_worker.get());
  return rc;
}

// Add all scheduling groups in `victims` to fiber workers in `thieves`.
//
// Even if scheduling the thief is inside presents in `victims`, it won't be
// added to the corresponding thief.
void InitializeForeignSchedulingGroups(
    const std::vector<std::unique_ptr<FullyFledgedSchedulingGroup>>& thieves,
    const std::vector<std::unique_ptr<FullyFledgedSchedulingGroup>>& victims,
    std::uint64_t steal_every_n) {
  for (std::size_t thief = 0; thief != thieves.size(); ++thief) {
    for (std::size_t victim = 0; victim != victims.size(); ++victim) {
      if (thieves[thief]->scheduling_group ==
          victims[victim]->scheduling_group) {
        return;
      }
      for (auto&& e : thieves[thief]->fiber_workers) {
        ExecuteWithAffinity(thieves[thief]->scheduling_group->Affinity(), [&] {
          e->AddForeignSchedulingGroup(victims[victim]->scheduling_group.get(),
                                       steal_every_n);
        });
      }
    }
  }
}

void StartWorkersUma() {
  FLARE_LOG_INFO(
      "Starting {} worker threads per group, for a total of {} groups. The "
      "system is treated as UMA.",
      scheduling_parameters.workers_per_group,
      scheduling_parameters.scheduling_groups);
  FLARE_LOG_WARNING_IF(
      FLAGS_flare_fiber_worker_disallow_cpu_migration &&
          GetFiberWorkerAccessibleNodes().size() > 1,
      "CPU migration of fiber worker is disallowed, and we're trying to start "
      "in UMA way on NUMA architecture. Performance will likely degrade.");

  for (std::size_t index = 0; index != scheduling_parameters.scheduling_groups;
       ++index) {
    if (!FLAGS_flare_fiber_worker_disallow_cpu_migration) {
      scheduling_groups[0].push_back(CreateFullyFledgedSchedulingGroup(
          0 /* Not sigfinicant */, GetFiberWorkerAccessibleCPUs(),
          scheduling_parameters.workers_per_group));
    } else {
      // Each group of processors is dedicated to a scheduling group.
      //
      // Later when we start the fiber workers, we'll instruct them to set their
      // affinity to their dedicated processors.
      auto&& cpus = GetFiberWorkerAccessibleCPUs();
      FLARE_CHECK_LE((index + 1) * scheduling_parameters.workers_per_group,
                     cpus.size());
      scheduling_groups[0].push_back(CreateFullyFledgedSchedulingGroup(
          0,
          {cpus.begin() + index * scheduling_parameters.workers_per_group,
           cpus.begin() +
               (index + 1) * scheduling_parameters.workers_per_group},
          scheduling_parameters.workers_per_group));
    }
  }

  InitializeForeignSchedulingGroups(scheduling_groups[0], scheduling_groups[0],
                                    FLAGS_flare_work_stealing_ratio);
}

void StartWorkersNuma() {
  auto topo = GetFiberWorkerAccessibleNodes();
  FLARE_CHECK_LT(topo.size(), std::size(scheduling_groups),
                 "Far more nodes that Flare can support present on this "
                 "machine. Bail out.");

  auto groups_per_node = scheduling_parameters.scheduling_groups / topo.size();
  FLARE_LOG_INFO(
      "Starting {} worker threads per group, {} group per node, for a total of "
      "{} nodes.",
      scheduling_parameters.workers_per_group, groups_per_node, topo.size());

  for (int i = 0; i != topo.size(); ++i) {
    for (int j = 0; j != groups_per_node; ++j) {
      if (!FLAGS_flare_fiber_worker_disallow_cpu_migration) {
        auto&& affinity = topo[i].logical_cpus;
        ExecuteWithAffinity(affinity, [&] {
          scheduling_groups[i].push_back(CreateFullyFledgedSchedulingGroup(
              i, affinity, scheduling_parameters.workers_per_group));
        });
      } else {
        // Same as `StartWorkersUma()`, fiber worker's affinity is set upon
        // start.
        auto&& cpus = topo[i].logical_cpus;
        FLARE_CHECK_LE((j + 1) * groups_per_node, cpus.size());
        std::vector<int> affinity = {
            cpus.begin() + j * scheduling_parameters.workers_per_group,
            cpus.begin() + (j + 1) * scheduling_parameters.workers_per_group};
        ExecuteWithAffinity(affinity, [&] {
          scheduling_groups[i].push_back(CreateFullyFledgedSchedulingGroup(
              i, affinity, scheduling_parameters.workers_per_group));
        });
      }
    }
  }

  for (int i = 0; i != topo.size(); ++i) {
    for (int j = 0; j != topo.size(); ++j) {
      if (FLAGS_flare_cross_numa_work_stealing_ratio == 0 && i != j) {
        // Different NUMA domain.
        //
        // `flare_enable_cross_numa_work_stealing` is not enabled, so we skip
        // this pair.
        continue;
      }
      InitializeForeignSchedulingGroups(
          scheduling_groups[i], scheduling_groups[j],
          i == j ? FLAGS_flare_work_stealing_ratio
                 : FLAGS_flare_cross_numa_work_stealing_ratio);
    }
  }
}

std::vector<int> GetFiberWorkerAccessibleCPUsImpl() {
  FLARE_CHECK(FLAGS_flare_fiber_worker_accessible_cpus.empty() ||
                  FLAGS_flare_fiber_worker_inaccessible_cpus.empty(),
              "At most one of `flare_fiber_worker_accessible_cpus` or "
              "`flare_fiber_worker_inaccessible_cpus` may be specified.");

  // If user specified accessible CPUs explicitly.
  if (!FLAGS_flare_fiber_worker_accessible_cpus.empty()) {
    auto procs = flare::internal::TryParseProcesserList(
        FLAGS_flare_fiber_worker_accessible_cpus);
    FLARE_CHECK(procs, "Failed to parse `flare_fiber_worker_accessible_cpus`.");
    return *procs;
  }

  // If affinity is set on the process, show our respect.
  //
  // Note that we don't have to do some dirty trick to check if processors we
  // get from affinity is accessible to us -- Inaccessible CPUs shouldn't be
  // returned to us in the first place.
  auto accessible_cpus = GetCurrentThreadAffinity();
  FLARE_CHECK(!accessible_cpus.empty());

  // If user specified inaccessible CPUs explicitly.
  if (!FLAGS_flare_fiber_worker_inaccessible_cpus.empty()) {
    auto option = flare::internal::TryParseProcesserList(
        FLAGS_flare_fiber_worker_inaccessible_cpus);
    FLARE_CHECK(option,
                "Failed to parse `flare_fiber_worker_inaccessible_cpus`.");
    std::set<int> inaccessible(option->begin(), option->end());

    // Drop processors we're forbidden to access.
    accessible_cpus.erase(
        std::remove_if(accessible_cpus.begin(), accessible_cpus.end(),
                       [&](auto&& x) { return inaccessible.count(x) != 0; }),
        accessible_cpus.end());
    return accessible_cpus;
  }

  // Thread affinity is respected implicitly.
  return accessible_cpus;
}

const std::vector<int>& GetFiberWorkerAccessibleCPUs() {
  static auto result = GetFiberWorkerAccessibleCPUsImpl();
  return result;
}

const std::vector<internal::numa::Node>& GetFiberWorkerAccessibleNodes() {
  static std::vector<internal::numa::Node> result = [] {
    std::map<int, std::vector<int>> node_to_processor;
    for (auto&& e : GetFiberWorkerAccessibleCPUs()) {
      auto n = internal::numa::GetNodeOfProcessor(e);
      node_to_processor[n].push_back(e);
    }

    std::vector<internal::numa::Node> result;
    for (auto&& [k, v] : node_to_processor) {
      result.push_back({k, v});
    }
    return result;
  }();
  return result;
}

void DisallowProcessorMigrationPreconditionCheck() {
  auto expected_concurrency =
      DivideRoundUp(fiber_concurrency_in_effect,
                    scheduling_parameters.workers_per_group) *
      scheduling_parameters.workers_per_group;
  FLARE_LOG_FATAL_IF(
      expected_concurrency > GetFiberWorkerAccessibleCPUs().size() &&
          FLAGS_flare_fiber_worker_disallow_cpu_migration,
      "CPU migration of fiber workers is explicitly disallowed, but there "
      "isn't enough CPU to dedicate one for each fiber worker. {} CPUs got, at "
      "least {} CPUs required.",
      GetFiberWorkerAccessibleCPUs().size(), expected_concurrency);
}

}  // namespace

namespace detail {

std::size_t GetCurrentSchedulingGroupIndexSlow() {
  auto rc = detail::NearestSchedulingGroupIndex();
  FLARE_CHECK(rc != -1,
              "Calling `GetCurrentSchedulingGroupIndex` outside of any "
              "scheduling group is undefined.");
  return rc;
}

std::optional<SchedulingProfile> GetSchedulingProfile() {
  auto customized =
      !google::GetCommandLineFlagInfoOrDie("flare_scheduling_group_size")
           .is_default ||
      !google::GetCommandLineFlagInfoOrDie("flare_numa_aware").is_default;
  if (customized) {
    // Keep the old behavior if the user specified the parameters manually.
    FLARE_LOG_ERROR_IF(
        FLAGS_flare_fiber_scheduling_optimize_for != "customized",
        "Flag `flare_scheduling_group_size` and `flare_numa_aware` are only "
        "respected if `customized` scheduling optimization strategy is used. "
        "We're still respecting your parameters to keep the old behavior. Try "
        "set `flare_fiber_scheduling_optimize_for` to `customized` to suppress "
        "this error.");
    return std::nullopt;
  }

  static const std::unordered_map<std::string, SchedulingProfile> kProfiles = {
      {"compute-heavy", SchedulingProfile::ComputeHeavy},
      {"compute", SchedulingProfile::Compute},
      {"neutral", SchedulingProfile::Neutral},
      {"io", SchedulingProfile::Io},
      {"io-heavy", SchedulingProfile::IoHeavy}};
  auto key = ToLower(FLAGS_flare_fiber_scheduling_optimize_for);
  FLARE_LOG_INFO("Using fiber scheduling profile [{}].", key);

  if (auto iter = kProfiles.find(key); iter != kProfiles.end()) {
    return iter->second;
  }
  if (key == "customized") {
    return std::nullopt;
  }
  FLARE_LOG_FATAL(
      "Unrecognized value for `--flare_fiber_scheduling_optimize_for`: [{}]",
      FLAGS_flare_fiber_scheduling_optimize_for);
}

void InitializeSchedulingParametersFromFlags() {
  auto profile = GetSchedulingProfile();
  fiber_concurrency_in_effect =
      FLAGS_flare_concurrency_hint ? FLAGS_flare_concurrency_hint
                                   : internal::GetNumberOfProcessorsAvailable();

  if (profile) {
    scheduling_parameters = GetSchedulingParameters(
        *profile, internal::numa::GetNumberOfNodesAvailable(),
        internal::GetNumberOfProcessorsAvailable(),
        fiber_concurrency_in_effect);
    return;
  }
  std::size_t groups =
      (fiber_concurrency_in_effect + FLAGS_flare_scheduling_group_size - 1) /
      FLAGS_flare_scheduling_group_size;
  scheduling_parameters = SchedulingParameters{
      .scheduling_groups = groups,
      .workers_per_group = (fiber_concurrency_in_effect + groups - 1) / groups,
      .enable_numa_affinity = FLAGS_flare_numa_aware};
}

}  // namespace detail

void StartRuntime() {
  // Get our final decision for scheduling parameters.
  detail::InitializeSchedulingParametersFromFlags();

  // If CPU migration is explicit disallowed, we need to make sure there are
  // enough CPUs for us.
  DisallowProcessorMigrationPreconditionCheck();

  if (scheduling_parameters.enable_numa_affinity) {
    StartWorkersNuma();
  } else {
    StartWorkersUma();
  }

  // Fill `flatten_scheduling_groups`.
  for (auto&& e : scheduling_groups) {
    for (auto&& ee : e) {
      flatten_scheduling_groups.push_back(ee.get());
    }
  }

  // Start the workers.
  for (auto&& e : scheduling_groups) {
    for (auto&& ee : e) {
      ee->Start(FLAGS_flare_fiber_worker_disallow_cpu_migration);
    }
  }
}

void TerminateRuntime() {
  for (auto&& e : scheduling_groups) {
    for (auto&& ee : e) {
      ee->Stop();
    }
  }
  for (auto&& e : scheduling_groups) {
    for (auto&& ee : e) {
      ee->Join();
    }
  }
  for (auto&& e : scheduling_groups) {
    e.clear();
  }
  flatten_scheduling_groups.clear();
}

std::size_t GetSchedulingGroupCount() {
  return flatten_scheduling_groups.size();
}

std::size_t GetSchedulingGroupSize() {
  return scheduling_parameters.workers_per_group;
}

int GetSchedulingGroupAssignedNode(std::size_t sg_index) {
  FLARE_CHECK_LT(sg_index, flatten_scheduling_groups.size());
  return flatten_scheduling_groups[sg_index]->node_id;
}

namespace detail {

SchedulingGroup* GetSchedulingGroup(std::size_t index) {
  FLARE_CHECK_LT(index, flatten_scheduling_groups.size());
  return flatten_scheduling_groups[index]->scheduling_group.get();
}

SchedulingGroup* NearestSchedulingGroupSlow(SchedulingGroup** cache) {
  if (auto rc = SchedulingGroup::Current()) {
    // Only if we indeed belong to the scheduling group (in which case the
    // "nearest" scheduling group never changes) we fill the cache.
    *cache = rc;
    return rc;
  }

  // We don't pay for overhead of initialize `next` unless we're not in running
  // fiber worker.
  FLARE_INTERNAL_TLS_MODEL thread_local std::size_t next = Random();

  auto&& current_node =
      scheduling_groups[scheduling_parameters.enable_numa_affinity
                            ? internal::numa::GetCurrentNode()
                            : 0];
  if (!current_node.empty()) {
    return current_node[next++ % current_node.size()]->scheduling_group.get();
  }

  if (!flatten_scheduling_groups.empty()) {
    return flatten_scheduling_groups[next++ % flatten_scheduling_groups.size()]
        ->scheduling_group.get();
  }

  return nullptr;
}

std::ptrdiff_t NearestSchedulingGroupIndex() {
  FLARE_INTERNAL_TLS_MODEL thread_local auto cached = []() -> std::ptrdiff_t {
    auto sg = NearestSchedulingGroup();
    if (sg) {
      auto iter = std::find_if(
          flatten_scheduling_groups.begin(), flatten_scheduling_groups.end(),
          [&](auto&& e) { return e->scheduling_group.get() == sg; });
      FLARE_CHECK(iter != flatten_scheduling_groups.end());
      return iter - flatten_scheduling_groups.begin();
    }
    return -1;
  }();
  return cached;
}

}  // namespace detail

}  // namespace flare::fiber
