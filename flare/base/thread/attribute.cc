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

#include "flare/base/thread/attribute.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <pthread.h>
#include <sched.h>

#include <string>

#include "flare/base/logging.h"

namespace flare {

int TrySetCurrentThreadAffinity(const std::vector<int>& affinity) {
  FLARE_CHECK(!affinity.empty());
  cpu_set_t cpuset;

  CPU_ZERO(&cpuset);
  for (auto&& e : affinity) {
    CPU_SET(e, &cpuset);
  }
  return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

void SetCurrentThreadAffinity(const std::vector<int>& affinity) {
  auto rc = TrySetCurrentThreadAffinity(affinity);
  FLARE_CHECK(rc == 0, "Cannot set thread affinity for thread [{}]: [{}] {}.",
              reinterpret_cast<const void*>(pthread_self()), rc, strerror(rc));
}

std::vector<int> GetCurrentThreadAffinity() {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  auto rc = pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
  FLARE_CHECK(rc == 0, "Cannot get thread affinity of thread [{}]: [{}] {}.",
              reinterpret_cast<const void*>(pthread_self()), rc, strerror(rc));

  std::vector<int> result;
  for (int i = 0; i != __CPU_SETSIZE; ++i) {
    if (CPU_ISSET(i, &cpuset)) {
      result.push_back(i);
    }
  }
  return result;
}

void SetCurrentThreadName(const std::string& name) {
  auto rc = pthread_setname_np(pthread_self(), name.c_str());
  if (rc != 0) {
    FLARE_LOG_WARNING("Cannot set name for thread [{}]: [{}] {}",
                      reinterpret_cast<const void*>(pthread_self()), rc,
                      strerror(rc));
    // Silently ignored.
  }
}

}  // namespace flare
