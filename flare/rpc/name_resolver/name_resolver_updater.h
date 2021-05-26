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

#ifndef FLARE_RPC_NAME_RESOLVER_NAME_RESOLVER_UPDATER_H_
#define FLARE_RPC_NAME_RESOLVER_NAME_RESOLVER_UPDATER_H_

#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

#include "flare/base/function.h"

namespace flare::name_resolver {

// A tool class to update route table periodically.
class NameResolverUpdater {
  struct UpdaterInfo {
    UpdaterInfo(Function<void()> up, std::chrono::seconds inter)
        : updater(std::move(up)), interval(inter) {
      update_time = std::chrono::steady_clock::now();
    }
    Function<void()> updater;
    const std::chrono::seconds interval;
    std::chrono::steady_clock::time_point update_time;
  };

 public:
  NameResolverUpdater();
  ~NameResolverUpdater();
  void Stop();
  void Register(const std::string& name, Function<void()> updater,
                std::chrono::seconds interval);

 private:
  void Start();
  void WorkProc();

 private:
  std::mutex updater_mutex_;
  std::condition_variable cond_;
  std::unordered_map<std::string, UpdaterInfo> updater_;

  std::atomic<bool> stopped_{false};
  std::thread work_waiter_;
};

}  // namespace flare::name_resolver

#endif  // FLARE_RPC_NAME_RESOLVER_NAME_RESOLVER_UPDATER_H_
