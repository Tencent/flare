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

#ifndef FLARE_BASE_MONITORING_MONITORING_SYSTEM_H_
#define FLARE_BASE_MONITORING_MONITORING_SYSTEM_H_

#include <memory>
#include <string>
#include <vector>

#include "flare/base/dependency_registry.h"
#include "flare/base/monitoring/event.h"

namespace flare::monitoring {

// Interface of monitoring system. Each monitoring system should provide their
// own implementation.
//
// The implementation should be thread-safe (for obvious reasons.).
class MonitoringSystem {
 public:
  virtual ~MonitoringSystem() = default;

  struct Personality {
    std::chrono::nanoseconds minimum_report_interval;
  };

  struct EventBuffers {
    std::vector<Event> discrete_events;
    std::vector<CoalescedCounterEvent> counter_events;
    std::vector<CoalescedGaugeEvent> gauge_events;
    std::vector<CoalescedTimerEvent> timer_events;
  };

  // Get personality of this monitoring system. Personality determines how
  // should Flare interact with the provider.
  virtual const Personality& GetPersonality() const = 0;

  // Report events to the monitoring system.
  virtual void Report(const EventBuffers& events) = 0;
};

FLARE_DECLARE_OBJECT_DEPENDENCY_REGISTRY(monitoring_system_registry,
                                         MonitoringSystem);

}  // namespace flare::monitoring

#define FLARE_MONITORING_REGISTER_MONITORING_SYSTEM(Name, Implementation) \
  FLARE_REGISTER_OBJECT_DEPENDENCY(                                       \
      flare::monitoring::monitoring_system_registry, Name,                \
      [] { return std::make_unique<Implementation>(); })

#endif  // FLARE_BASE_MONITORING_MONITORING_SYSTEM_H_
