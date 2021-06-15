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

#include "flare/base/internal/cpu.h"

#include <dlfcn.h>
#include <sys/sysinfo.h>
#include <syscall.h>
#include <unistd.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "gflags/gflags.h"
#include "glog/raw_logging.h"

#include "flare/base/internal/logging.h"
#include "flare/base/string.h"
#include "flare/base/thread/attribute.h"

DEFINE_bool(flare_ignore_inaccessible_cpus, true,
            "During enumerating CPUs, in case there are CPU(s) to which we "
            "cannot bind our pthread workers, this flag controls the action we "
            "take. [DEPRECATED: This flag is no longer respected, and Flare "
            "will always behaves as if the flag is set.].");

namespace flare::internal {

namespace {

// Initialized by `InitializeProcessorInfoOnce()`.
bool inaccessible_cpus_present{false};
std::vector<int> node_of_cpus;
std::vector<std::size_t> node_index;
std::vector<int> nodes_present;

bool IsValgrindPresent() {
  // You need to export this var yourself in bash.
  char* rov = getenv("RUNNING_ON_VALGRIND");
  if (rov) {
    return strcmp(rov, "0") != 0;
  }
  return false;
}

int SyscallGetCpu(unsigned* cpu, unsigned* node, void* cache) {
#ifndef SYS_getcpu
  FLARE_LOG_FATAL(
      "Not supported: sys_getcpu. Note that this is only required if you want "
      "to run Flare in valgrind or on some exotic ISAs.");
#else
  return syscall(SYS_getcpu, cpu, node, cache);
#endif
}

// @sa: https://gist.github.com/chergert/eb6149916b10d3bf094c
int (*GetCpu)(unsigned* cpu, unsigned* node, void* cache) = [] {
  if (IsValgrindPresent()) {
    return SyscallGetCpu;
  } else {
#if defined(__aarch64__)
    // `getcpu` is not available on AArch64.
    return SyscallGetCpu;
#endif
    // Not all ISAs use the same name here. We'll try our best to locate
    // `getcpu` via vDSO.
    //
    // @sa http://man7.org/linux/man-pages/man7/vdso.7.html for more details.

    static const char* kvDSONames[] = {"linux-gate.so.1", "linux-vdso.so.1",
                                       "linux-vdso32.so.1",
                                       "linux-vdso64.so.1"};
    static const char* kGetCpuNames[] = {"__vdso_getcpu", "__kernel_getcpu"};
    for (auto&& e : kvDSONames) {
      if (auto vdso = dlopen(e, RTLD_NOW)) {
        for (auto&& e2 : kGetCpuNames) {
          if (auto p = dlsym(vdso, e2)) {
            // AFAICT, leaking `vdso` does nothing harmful to us. We use a
            // managed pointer to hold it only to comfort Coverity. (But it
            // still doesn't seem comforttable with this..)
            [[maybe_unused]] static std::unique_ptr<void, int (*)(void*)>
                suppress_vdso_leak{vdso, &dlclose};

            return reinterpret_cast<int (*)(unsigned*, unsigned*, void*)>(p);
          }
        }
        dlclose(vdso);
      }
    }
    // Fall back to syscall. This can be slow.
    //
    // Using raw logging here as glog is unlikely to have been initialized now.
    RAW_LOG(WARNING,
            "Failed to locate `getcpu` in vDSO. Failing back to syscall. "
            "Performance will degrade.");
    return SyscallGetCpu;
  }
}();

int GetNodeOfProcessorImpl(int proc_id) {
  // Slow, indeed. We don't expect this method be called much.
  std::atomic<int> rc;
  std::thread([&] {
    if (auto err = TrySetCurrentThreadAffinity({proc_id}); err != 0) {
      FLARE_CHECK(err == EINVAL, "Unexpected error #{}: {}", err,
                  strerror(err));
      rc = -1;
      return;
    }
    unsigned cpu, node;
    FLARE_CHECK_EQ(GetCpu(&cpu, &node, nullptr), 0, "`GetCpu` failed.");
    rc = node;
  }).join();
  return rc.load();
}

// Initialize those global variables.
void InitializeProcessorInfoOnce() {
  // I don't think it's possible to print log reliably here, unfortunately.
  static std::once_flag once;
  std::call_once(once, [&] {
    node_of_cpus.resize(GetNumberOfProcessorsConfigured(), -1);
    for (int i = 0; i != node_of_cpus.size(); ++i) {
      auto n = GetNodeOfProcessorImpl(i);
      if (n == -1) {
        // Failed to determine the processor's belonging node.
        inaccessible_cpus_present = true;
      } else {
        if (node_index.size() < n + 1) {
          node_index.resize(n + 1, -1);
        }
        if (node_index[n] == -1) {
          // New node discovered.
          node_index[n] = nodes_present.size();
          nodes_present.push_back(n);
        }
        node_of_cpus[i] = n;
        // New processor discovered.
      }
    }
  });
}

struct ProcessorInfoInitializer {
  ProcessorInfoInitializer() { InitializeProcessorInfoOnce(); }
} processor_info_initializer [[maybe_unused]];

}  // namespace

namespace numa {

namespace {

std::vector<Node> GetAvailableNodesImpl() {
  // NUMA node -> vector of processor ID.
  std::unordered_map<int, std::vector<int>> desc;
  for (int index = 0; index != node_of_cpus.size(); ++index) {
    auto n = node_of_cpus[index];
    if (n == -1) {
      FLARE_LOG_WARNING_ONCE(
          "Cannot determine node ID of processor #{}, we're silently ignoring "
          "that CPU. Unless that CPU indeed shouldn't be used by the program "
          "(e.g., containerized environment or disabled), you should check the "
          "situation as it can have a negative impact on performance.",
          index);
      continue;
    }
    desc[n].push_back(index);
  }

  std::vector<Node> rc;
  for (auto&& id : nodes_present) {
    Node n;
    n.id = id;
    n.logical_cpus = desc[id];
    rc.push_back(n);
  }
  return rc;
}

}  // namespace

std::vector<Node> GetAvailableNodes() {
  static const auto rc = GetAvailableNodesImpl();
  return rc;
}

int GetCurrentNode() {
  unsigned cpu, node;

  // Another approach: https://stackoverflow.com/a/27450168
  FLARE_CHECK_EQ(0, GetCpu(&cpu, &node, nullptr), "Cannot get NUMA ID.");
  return node;
}

std::size_t GetCurrentNodeIndex() { return GetNodeIndex(GetCurrentNode()); }

int GetNodeId(std::size_t index) {
  FLARE_CHECK_LT(index, nodes_present.size());
  return nodes_present[index];
}

std::size_t GetNodeIndex(int node_id) {
  FLARE_CHECK_LT(node_id, node_index.size());
  FLARE_CHECK_LT(node_index[node_id], nodes_present.size());
  return node_index[node_id];
}

std::size_t GetNumberOfNodesAvailable() { return nodes_present.size(); }

int GetNodeOfProcessor(int cpu) {
  FLARE_CHECK_LE(cpu, node_of_cpus.size());
  auto node = node_of_cpus[cpu];
  FLARE_CHECK_NE(node, -1, "Processor #{} is not accessible.", cpu);
  return node;
}

}  // namespace numa

int GetCurrentProcessorId() {
  unsigned cpu, node;
  FLARE_CHECK_EQ(0, GetCpu(&cpu, &node, nullptr), "Cannot get current CPU ID.");
  return cpu;
}

std::size_t GetNumberOfProcessorsAvailable() {
  // We do not support CPU hot-plugin, so we use `static` here.
  static const auto rc = get_nprocs();
  return rc;
}

std::size_t GetNumberOfProcessorsConfigured() {
  // We do not support CPU hot-plugin, so we use `static` here.
  static const auto rc = get_nprocs_conf();
  return rc;
}

bool IsInaccessibleProcessorPresent() { return inaccessible_cpus_present; }

bool IsProcessorAccessible(int cpu) {
  FLARE_CHECK_LT(cpu, node_of_cpus.size());
  return node_of_cpus[cpu] != -1;
}

std::optional<std::vector<int>> TryParseProcesserList(const std::string& s) {
  std::vector<int> result;
  auto parts = Split(s, ",");
  for (auto&& e : parts) {
    auto id = TryParse<int>(e);
    if (id) {
      if (*id < 0) {
        result.push_back(flare::internal::GetNumberOfProcessorsConfigured() +
                         *id);
        if (result.back() < 0) {
          return std::nullopt;
        }
      } else {
        result.push_back(*id);
      }
    } else {
      auto range = Split(e, "-");
      if (range.size() != 2) {
        return std::nullopt;
      }
      auto s = TryParse<int>(range[0]), e = TryParse<int>(range[1]);
      if (!s || !e || *s > *e) {
        return std::nullopt;
      }
      for (int i = *s; i <= *e; ++i) {
        result.push_back(i);
      }
    }
  }
  return result;
}

}  // namespace flare::internal
