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

#include "flare/base/monitoring/init.h"

#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "gflags/gflags.h"
#include "yaml-cpp/yaml.h"

#include "flare/base/internal/hash_map.h"
#include "flare/base/monitoring/dispatcher.h"
#include "flare/base/monitoring/monitoring_system.h"
#include "flare/base/never_destroyed.h"
#include "flare/base/string.h"

DEFINE_string(flare_monitoring_system, "",
              "Monitoring system(s) to which values reported by utilities in "
              "`flare::monitoring::` are reported to. If you want to report to "
              "multiple monitoring systems simultaneously, you should list all "
              "monitoring systems you want to report to here. Monitoring "
              "systems should be separated by comma. (e.g.: `tnm,gxt`.)");
DEFINE_string(
    flare_monitoring_key_remap, "",
    "If you're unable to use same keys across multiple monitoring systems, or "
    "you're simply using different monitoring keys in code from what you've "
    "registered with the monitoring system, you can provide a remap file (by "
    "its path) here. If provided, the framework remaps keys specified in "
    "source code using mapping in the file before forwarding the events to "
    "actual monitoring system(s). If you're using multiple monitoring systems, "
    "remap files should be listed in style of "
    "`sys1=path/to/remap1.conf,sys2=path/to/remap2.conf`. You don't have to "
    "specify monitoring system name if you're only using a single monitoring "
    "system (i.e., only specifying file name is sufficient.). For "
    "configuration file format, you can checkout our documentation (in `doc/`) "
    "and examples in `testdata/`");
DEFINE_string(
    flare_monitoring_builtin_key_mapping, "",
    "If asked, Flare reports several aspects of its internals. However, Flare "
    "by itself can't tell to which key should it report. Therefore, we "
    "hardcoded monitoring key internally, and map the keys to whatever the "
    "user like using this file. See out documentation and examples in "
    "`testdata/` for configuration format. Keys used by Flare are remapped via "
    "this mapping first, and then via `flare_monitoring_key_remap` if that "
    "flag is set as well.");

namespace flare::monitoring {

namespace {

struct System {
  std::string name;
  MonitoringSystem* system;

  // Set if key-remap is configured.
  std::unique_ptr<internal::HashMap<std::string, std::string>> key_remap;
  bool passthrough_on_missing;
};

std::string GetRemapConfigOf(std::string_view name) {
  if (FLAGS_flare_monitoring_key_remap.find('=') != std::string::npos) {
    auto remaps = Split(FLAGS_flare_monitoring_key_remap, ",");
    for (auto&& e : remaps) {
      auto pairs = Split(Trim(e), "=");
      FLARE_CHECK_EQ(
          pairs.size(), 2,
          "Invalid `flare_monitoring_key_remap`. You should either provide a "
          "file name, or, in case you want to use different remap file for "
          "different monitoring systems, in style of "
          "`sys1=path/to/remap1.conf,sys2=path/to/remap2.conf,sys3=...`. "
          "Read [{}].",
          FLAGS_flare_monitoring_key_remap);
      if (pairs[0] == name) {
        return std::string(pairs[1]);
      }
    }
    return "";  // No remap file for it.
  } else {
    return FLAGS_flare_monitoring_key_remap;
  }
}

// Slow but works.
System LoadSystem(std::string_view name) {
  System system;

  system.name = std::string(name);
  system.system = monitoring_system_registry.Get(name);
  if (auto s = GetRemapConfigOf(name); !s.empty()) {
    FLARE_LOG_INFO("Using remapping file at [{}] for monitoring system [{}].",
                   s, name);

    YAML::Node config;
    try {
      config = YAML::LoadFile(s);
    } catch (const YAML::BadFile& xcpt) {
      FLARE_CHECK(0, "Failed to load remap file [{}]: {}", s, xcpt.what());
    }
    system.passthrough_on_missing = config["passthrough-on-missing"].as<bool>();

    system.key_remap =
        std::make_unique<internal::HashMap<std::string, std::string>>();
    auto keys = config["keys"];
    for (auto iter = keys.begin(); iter != keys.end(); ++iter) {
      auto&& k = iter->first.as<std::string>();
      auto&& v = iter->second.as<std::string>();
      FLARE_VLOG(10,
                 "Remapping key [{}] to [{}] when reporting event to "
                 "monitoring system [{}].",
                 k, v, name);
      (*system.key_remap)[k] = v;
    }
  } else {
    system.passthrough_on_missing = true;
  }
  return system;
}

// Set on initialization.
std::atomic<std::unordered_map<std::string, std::string>*> builtin_key_map{
    nullptr};

struct UnresolvedBuiltinMonitorKey {
  std::string key;
  Function<void(const std::string&)> cb;
};

auto* GetUnresolvedBuiltinMonitorKeyRegistry() {
  static NeverDestroyed<std::vector<UnresolvedBuiltinMonitorKey>> pending;
  return pending.Get();
}

void InitializeBuiltinKeyMapping() {
  if (FLAGS_flare_monitoring_builtin_key_mapping.empty()) {
    return;
  }

  {
    auto mapping =
        std::make_unique<std::unordered_map<std::string, std::string>>();

    YAML::Node config;
    try {
      config = YAML::LoadFile(FLAGS_flare_monitoring_builtin_key_mapping);
    } catch (const YAML::BadFile& xcpt) {
      FLARE_CHECK(0, "Failed to load mapping [{}]: {}",
                  FLAGS_flare_monitoring_builtin_key_mapping, xcpt.what());
    }

    auto keys = config["keys"];
    for (auto iter = keys.begin(); iter != keys.end(); ++iter) {
      auto&& k = iter->first.as<std::string>();
      auto&& v = iter->second.as<std::string>();
      FLARE_VLOG(10,
                 "Mapping builtin key [{}] to [{}] when reporting to "
                 "monitoring system.",
                 k, v);
      (*mapping)[k] = v;
    }

    builtin_key_map.store(mapping.release(), std::memory_order_release);
  }

  auto&& mapping = *builtin_key_map.load(std::memory_order_acquire);

  for (auto&& [key, cb] : *GetUnresolvedBuiltinMonitorKeyRegistry()) {
    if (auto iter = mapping.find(key); iter != mapping.end()) {
      cb(iter->second);
    } else {
      cb({});
    }
  }

  GetUnresolvedBuiltinMonitorKeyRegistry()->clear();
}

}  // namespace

void InitializeMonitoringSystem() {
  InitializeBuiltinKeyMapping();

  // Register monitoring system providers with `Dispatcher`.

  auto names = Split(FLAGS_flare_monitoring_system, ",");
  for (auto&& e : names) {
    auto name = Trim(e);
    auto sys = LoadSystem(name);
    if (sys.key_remap) {
      Dispatcher::Instance()->AddMonitoringSystem(sys.name, sys.system,
                                                  std::move(*sys.key_remap),
                                                  !sys.passthrough_on_missing);
    } else {
      Dispatcher::Instance()->AddMonitoringSystem(sys.name, sys.system);
    }
  }

  Dispatcher::Instance()->Start();
}

void TerminateMonitoringSystem() {
  Dispatcher::Instance()->Stop();
  Dispatcher::Instance()->Join();
}

void RegisterBuiltinMonitoringKeyCallback(
    const std::string& key, Function<void(const std::string&)> cb) {
  if (auto ptr = builtin_key_map.load(std::memory_order_acquire)) {
    if (auto iter = ptr->find(key); iter != ptr->end()) {
      cb(iter->second);
    } else {
      cb({});
    }
  } else {
    // Remapping is not initialized yet. We must be in pre-`main` environment.
    // No locking is required then.
    GetUnresolvedBuiltinMonitorKeyRegistry()->emplace_back(
        UnresolvedBuiltinMonitorKey{key, std::move(cb)});
  }
}

}  // namespace flare::monitoring
