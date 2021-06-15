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

#include "flare/rpc/binlog/dry_runner.h"

#include <memory>

#include "gflags/gflags.h"

#include "flare/base/never_destroyed.h"
#include "flare/base/string.h"

using namespace std::literals;

DEFINE_string(flare_binlog_dry_runner, "",
              "Name of binlog dry-runner. To do a dry-run with binlogs dumped "
              "before (presumably in production environment), you should use "
              "the dry-runner shipped together with the dumper you were used. "
              "It's almost always an error to use a dry-runner that is not "
              "paired with the dumper who wrote the binlog.");

namespace flare::binlog {

FLARE_DEFINE_CLASS_DEPENDENCY_REGISTRY(dry_runner_registry, DryRunner);

namespace {

std::unique_ptr<DryRunner> CreateDryRunnerFromFlags() {
  if (FLAGS_flare_binlog_dry_runner.empty()) {
    return nullptr;
  }
  FLARE_LOG_INFO("Using [{}] to perform dry-run.",
                 FLAGS_flare_binlog_dry_runner);
  auto v = dry_runner_registry.New(FLAGS_flare_binlog_dry_runner);
  return v;
}

}  // namespace

DryRunner* GetDryRunner() {
  static NeverDestroyed<std::unique_ptr<DryRunner>> runner(
      CreateDryRunnerFromFlags());
  return runner->get();
}

}  // namespace flare::binlog
