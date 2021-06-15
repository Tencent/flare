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

#include "flare/rpc/binlog/init.h"

#include "gflags/gflags.h"

#include "flare/rpc/binlog/dry_runner.h"
#include "flare/rpc/binlog/dumper.h"

DECLARE_string(flare_binlog_dumper);
DECLARE_string(flare_binlog_dry_runner);

namespace flare::binlog {

void InitializeBinlog() {
  FLARE_CHECK(FLAGS_flare_binlog_dumper.empty() ||
                  FLAGS_flare_binlog_dry_runner.empty(),
              "Both `flare_binlog_dumper` and `flare_binlog_dry_runner` it "
              "specified. They cannot be enabled at the same time.");

  (void)GetDumper();
  (void)GetDryRunner();
}

}  // namespace flare::binlog
