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

#include "flare/rpc/name_resolver/name_resolver_updater.h"

#include <string>
#include <utility>
#include <vector>

#include "flare/base/logging.h"
#include "flare/base/thread/attribute.h"

using namespace std::literals;

namespace flare::name_resolver {

NameResolverUpdater::NameResolverUpdater() { Start(); }

NameResolverUpdater::~NameResolverUpdater() { Stop(); }

void NameResolverUpdater::Stop() {
  if (!std::atomic_exchange(&stopped_, true)) {
    cond_.notify_one();
    work_waiter_.join();
  }
}

void NameResolverUpdater::Start() {
  work_waiter_ = std::thread([this]() {
    SetCurrentThreadName("NameResolverUp");
    WorkProc();
  });
}

void NameResolverUpdater::Register(const std::string& address,
                                   Function<void()> updater,
                                   std::chrono::seconds interval) {
  std::scoped_lock lk(updater_mutex_);
  if (auto iter = updater_.find(address); iter == updater_.end()) {
    updater_.emplace(std::piecewise_construct, std::forward_as_tuple(address),
                     std::forward_as_tuple(std::move(updater), interval));
  } else {
    FLARE_LOG_ERROR("Duplicate Register for address {}", address);
  }
}

void NameResolverUpdater::WorkProc() {
  while (!stopped_.load(std::memory_order_relaxed)) {
    auto now = std::chrono::steady_clock::now();
    std::vector<UpdaterInfo*> need_update;
    {
      std::scoped_lock lk(updater_mutex_);
      for (auto&& [address, updater_info] : updater_) {
        if (now - updater_info.update_time > updater_info.interval) {
          need_update.push_back(&updater_info);
        }
      }
    }

    for (auto&& updater_info : need_update) {
      updater_info->updater();  // may be blocking
      updater_info->update_time = std::chrono::steady_clock::now();
    }

    std::unique_lock lk(updater_mutex_);
    cond_.wait_for(lk, 1s,
                   [this] { return stopped_.load(std::memory_order_relaxed); });
  }
}

}  // namespace flare::name_resolver
