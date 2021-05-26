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

#ifndef FLARE_BASE_MONITORING_DISPATCHER_H_
#define FLARE_BASE_MONITORING_DISPATCHER_H_

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "flare/base/internal/hash_map.h"
#include "flare/base/monitoring/event.h"

namespace flare::monitoring {

class MonitoringSystem;

// The dispatcher is responsible for merging and reporting events to monitoring
// system providers.
//
// It's also responsible for throttling the frequency of calling monitoring
// system's provider if required.
//
// All the heavy lifting method is called in the background threads (by the
// caller), so we care about performance too much.
class Dispatcher {
 public:
  static Dispatcher* Instance();

  // Report event to our monitoring subsystem.
  //
  // The data reported will be handled to monitoring system provider eventually
  // (but not necessarily immediately).
  void ReportEvents(const std::vector<Event>& events);
  void ReportCoalescedEvent(const CoalescedCounterEvent& event);
  void ReportCoalescedEvent(const CoalescedGaugeEvent& event);
  void ReportCoalescedEvent(const CoalescedTimerEvent& event);

  // Register a monitoring system provider. If `key_mapping` is provided. the
  // dispatcher remaps monitoring keys before reporting the events.
  void AddMonitoringSystem(std::string name, MonitoringSystem* system);
  void AddMonitoringSystem(
      std::string name, MonitoringSystem* system,
      internal::HashMap<std::string, std::string> key_mapping,
      bool drop_unknown_keys);

  void Start();
  void Stop();
  void Join();

 private:
  void OnTimerProc();

 private:
  // (Key, tags). Arguably it's rather slow. But the dispatcher won't be called
  // to often anyway.
  using MapKey =
      std::pair<std::string, std::vector<std::pair<std::string, std::string>>>;

  struct PendingEvents {
    std::mutex lock;
    std::chrono::steady_clock::time_point next_flush_at{
        ReadCoarseSteadyClock()};
    std::vector<Event> discrete_events;
    std::map<MapKey, CoalescedCounterEvent> counter_events;
    std::map<MapKey, CoalescedGaugeEvent> gauge_events;
    std::map<MapKey, CoalescedTimerEvent> timer_events;
  };

  struct PerSystemEvents {
    std::string name;
    MonitoringSystem* system;
    std::unique_ptr<PendingEvents> events;
    bool remap_keys;
    internal::HashMap<std::string, std::string> key_mapping;
    bool drop_unknown_keys;
  };

  std::uint64_t timer_id_;

  // Not protected by lock, all calls to `AddMonitoringSystem` must be done at
  // initialization time.
  std::vector<PerSystemEvents> pending_events_;
};

}  // namespace flare::monitoring

#endif  // FLARE_BASE_MONITORING_DISPATCHER_H_
