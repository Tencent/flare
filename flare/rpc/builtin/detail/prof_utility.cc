// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "flare/rpc/builtin/detail/prof_utility.h"

#include <stdio.h>

#include "flare/base/logging.h"

namespace flare::rpc::builtin {

std::string ReadProcPath() {
  static constexpr int kLength = 512;
  char cmdline[kLength];
  ssize_t nr = readlink("/proc/self/exe", cmdline, kLength - 1);
  if (nr <= 0) {
    FLARE_LOG_ERROR("Fail to read /proc/self/exe");
    return "";
  }

  if (static_cast<size_t>(nr) == kLength - 1) {
    FLARE_LOG_ERROR("Buf is not big enough");
    return "";
  }
  return std::string(cmdline, nr);
}

bool PopenNoShellCompat(const std::string& command, std::string* result,
                        int* exit_code) {
  result->clear();
  FILE* fp = popen(command.c_str(), "r");
  if (!fp) {
    return false;
  }
  char buf[8 * 1024];
  while (fgets(buf, sizeof(buf) - 1, fp)) {
    *result += buf;
  }
  int ret = pclose(fp);
  if (exit_code) {
    *exit_code = ret;
  }
  return true;
}

}  // namespace flare::rpc::builtin
