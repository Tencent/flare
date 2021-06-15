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

#include "flare/init/override_flag.h"

#include <string>
#include <unordered_map>
#include <utility>

#include "gflags/gflags.h"
#include "glog/raw_logging.h"

#include "flare/base/logging.h"
#include "flare/base/never_destroyed.h"

namespace flare::detail {

namespace {

using Registry = std::unordered_map<std::string, std::pair<std::string, bool>>;

Registry* GetRegistry() {
  static NeverDestroyed<Registry> registry;
  return registry.Get();
}

}  // namespace

void RegisterFlagOverrider(const std::string& name, const std::string& to,
                           bool forcibly) {
  auto&& value = (*GetRegistry())[name];
  if (!value.first.empty()) {
    RAW_LOG_FATAL("Duplicate override for flag %s, was [%s], now [%s]",
                  name.c_str(), value.first.c_str(), to.c_str());
  }
  value = std::pair(to, forcibly);
}

// Called by `flare::Start`.
void ApplyFlagOverrider() {
  for (auto&& [k, v] : *GetRegistry()) {
    // Make sure the flag name is present.
    auto current = google::GetCommandLineFlagInfoOrDie(k.c_str());
    if (current.is_default || v.second) {
      google::SetCommandLineOption(k.c_str(), v.first.c_str());
      FLARE_VLOG(10, "Overriding flag [{}] with [{}].", k, v.first);
    }
  }
}

}  // namespace flare::detail
