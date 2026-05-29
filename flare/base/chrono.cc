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

#include "flare/base/chrono.h"

#include <chrono>
#include <thread>

using namespace std::literals;

namespace flare {

namespace detail::chrono {

void UpdateCoarseTimestamps() {
  async_updated_timestamps.steady_clock_time_since_epoch.store(
      std::chrono::steady_clock::now().time_since_epoch(),
      std::memory_order_relaxed);
  async_updated_timestamps.system_clock_time_since_epoch.store(
      std::chrono::system_clock::now().time_since_epoch(),
      std::memory_order_relaxed);
}

CoarseClockInitializer::CoarseClockInitializer() {
  UpdateCoarseTimestamps();  // Initialize timestamp immediately.

  // Indeed we _can_ use `TimeKeeper` here to register a timer. However, that
  // simply carries too many dependencies to the rest of the framework. To keep
  // things simple, we use a dedicated thread here.
  worker_ = std::thread([this] {
    while (!exiting_.load(std::memory_order_relaxed)) {
      std::this_thread::sleep_for(4ms);
      UpdateCoarseTimestamps();
    }
  });
}

CoarseClockInitializer::~CoarseClockInitializer() {
  exiting_.store(true, std::memory_order_relaxed);
  worker_.join();
}

}  // namespace detail::chrono

}  // namespace flare
