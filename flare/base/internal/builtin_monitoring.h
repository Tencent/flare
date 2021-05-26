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

#ifndef FLARE_BASE_INTERNAL_BUILTIN_MONITORING_H_
#define FLARE_BASE_INTERNAL_BUILTIN_MONITORING_H_

#include <string>
#include <utility>

#include "flare/base/delayed_init.h"
#include "flare/base/monitoring.h"
#include "flare/base/monitoring/init.h"

// FOR INTERNAL USE ONLY.
//
// Flare can report some of its internal state to monitoring systems. Utilities
// here help Flare to implement this.

namespace flare::internal {

// @sa: `MonitoredCounter`.
class BuiltinMonitoredCounter {
 public:
  // Tests if we should be enabled and initializes implementation.
  template <class... Ts>
  explicit BuiltinMonitoredCounter(std::string key, Ts&&... args);

  // Forwards calls to the implementation if we're enabled.
  template <class... Ts>
  void Add(Ts&&... args);
  template <class... Ts>
  void Increment(Ts&&... args);

 private:
  void SetupDelayedInitialization(const std::string& key);

 private:
  DelayedInit<MonitoredCounter> impl_;
};

// @sa: MonitoredGauge`.
class BuiltinMonitoredGauge {
 public:
  // Tests if we should be enabled and initializes implementation.
  template <class... Ts>
  explicit BuiltinMonitoredGauge(std::string key, Ts&&... args);

  // Forwards calls to the implementation if we're enabled.
  template <class... Ts>
  void Add(Ts&&... args);
  template <class... Ts>
  void Subtract(Ts&&... args);
  template <class... Ts>
  void Increment(Ts&&... args);
  template <class... Ts>
  void Decrement(Ts&&... args);

 private:
  void SetupDelayedInitialization(const std::string& key);

 private:
  DelayedInit<MonitoredGauge> impl_;
};

// @sa: MonitoredTimer`.
class BuiltinMonitoredTimer {
 public:
  // Tests if we should be enabled and initializes implementation.
  template <class... Ts>
  explicit BuiltinMonitoredTimer(std::string key, Ts&&... args);

  // Forwards calls to the implementation if we're enabled.
  template <class... Ts>
  void Report(Ts&&... args);

 private:
  void SetupDelayedInitialization(const std::string& key);

 private:
  DelayedInit<MonitoredTimer> impl_;
};

template <class... Ts>
BuiltinMonitoredCounter::BuiltinMonitoredCounter(std::string key,
                                                 Ts&&... args) {
  auto init_cb = [this, args...](auto&& remapped_key) {
    if (!remapped_key.empty()) {
      impl_.Init(remapped_key, args...);
    }
  };
  monitoring::RegisterBuiltinMonitoringKeyCallback(key, init_cb);
}

template <class... Ts>
void BuiltinMonitoredCounter::Add(Ts&&... args) {
  if (impl_) {
    impl_->Add(std::forward<Ts>(args)...);
  }
}

template <class... Ts>
void BuiltinMonitoredCounter::Increment(Ts&&... args) {
  if (impl_) {
    impl_->Increment(std::forward<Ts>(args)...);
  }
}

template <class... Ts>
BuiltinMonitoredGauge::BuiltinMonitoredGauge(std::string key, Ts&&... args) {
  auto init_cb = [this, args...](auto&& remapped_key) {
    if (!remapped_key.empty()) {
      impl_.Init(remapped_key, args...);
    }
  };
  monitoring::RegisterBuiltinMonitoringKeyCallback(key, init_cb);
}

template <class... Ts>
void BuiltinMonitoredGauge::Add(Ts&&... args) {
  if (impl_) {
    impl_->Add(std::forward<Ts>(args)...);
  }
}

template <class... Ts>
void BuiltinMonitoredGauge::Subtract(Ts&&... args) {
  if (impl_) {
    impl_->Subtract(std::forward<Ts>(args)...);
  }
}

template <class... Ts>
void BuiltinMonitoredGauge::Increment(Ts&&... args) {
  if (impl_) {
    impl_->Increment(std::forward<Ts>(args)...);
  }
}

template <class... Ts>
void BuiltinMonitoredGauge::Decrement(Ts&&... args) {
  if (impl_) {
    impl_->Decrement(std::forward<Ts>(args)...);
  }
}

template <class... Ts>
BuiltinMonitoredTimer::BuiltinMonitoredTimer(std::string key, Ts&&... args) {
  auto init_cb = [this, args...](auto&& remapped_key) {
    if (!remapped_key.empty()) {
      impl_.Init(remapped_key, args...);
    }
  };
  monitoring::RegisterBuiltinMonitoringKeyCallback(key, init_cb);
}

template <class... Ts>
void BuiltinMonitoredTimer::Report(Ts&&... args) {
  if (impl_) {
    impl_->Report(std::forward<Ts>(args)...);
  }
}

}  // namespace flare::internal

#endif  // FLARE_BASE_INTERNAL_BUILTIN_MONITORING_H_
