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

#include "flare/base/monitoring/dispatcher.h"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "flare/base/chrono.h"
#include "flare/base/internal/time_keeper.h"
#include "flare/base/monitoring/monitoring_system.h"
#include "flare/base/never_destroyed.h"

using namespace std::literals;

DEFINE_int32(
    flare_monitoring_minimum_report_interval, 1000,
    "Interval between two reports to monitoring system, in milliseconds. "
    "Setting it too low can hurt performance. Note that we don't always "
    "respect this parameter. If all monitoring systems in use require a "
    "greater \"minimum interval\", their request is respected instead.");

namespace flare::monitoring {

namespace {

template <class T>
std::optional<std::string> TryRemapKey(const T& config,
                                       const std::string_view& key) {
  if (!config.remap_keys) {
    return std::string(key);
  }
  if (auto opt = config.key_mapping.TryGet(key)) {
    return *opt;
  }
  if (!config.drop_unknown_keys) {
    return std::string(key);
  }
  FLARE_LOG_WARNING_ONCE(
      "At least one of keys reported (e.g. [{}]) is not found in remap file of "
      "monitoring system [{}]. Ignoring.",
      key, config.name);
  return std::nullopt;
}

template <class T, class U, class V>
void MoveReports(const V& config, T* to, U* from) {
  to->reserve(from->size());
  for (auto&& [key, events] : *from) {
    to->push_back(std::move(events));

    if (auto opt = TryRemapKey(config, key.first)) {
      to->back().key = *opt;
    } else {
      to->pop_back();
    }
  }
}

}  // namespace

Dispatcher* Dispatcher::Instance() {
  static NeverDestroyed<Dispatcher> dispatcher;
  return dispatcher.Get();
}

void Dispatcher::ReportEvents(const std::vector<Event>& events) {
  for (auto&& per_sys : pending_events_) {
    auto&& e = per_sys.events;
    std::scoped_lock _(e->lock);
    e->discrete_events.insert(e->discrete_events.end(), events.begin(),
                              events.end());
  }
}

void Dispatcher::ReportCoalescedEvent(const CoalescedCounterEvent& event) {
  for (auto&& per_sys : pending_events_) {
    auto&& e = per_sys.events;
    std::scoped_lock _(e->lock);
    auto&& key = std::pair(event.key, event.tags);
    auto iter = e->counter_events.find(key);
    if (iter == e->counter_events.end()) {
      e->counter_events[key] = event;
    } else {
      auto&& entry = iter->second;
      entry.sum += event.sum;
      entry.times += event.times;
    }
  }
}

void Dispatcher::ReportCoalescedEvent(const CoalescedGaugeEvent& event) {
  for (auto&& per_sys : pending_events_) {
    auto&& e = per_sys.events;
    std::scoped_lock _(e->lock);
    auto&& key = std::pair(event.key, event.tags);
    auto iter = e->gauge_events.find(key);
    if (iter == e->gauge_events.end()) {
      e->gauge_events[key] = event;
    } else {
      auto&& entry = iter->second;
      entry.sum += event.sum;
      entry.times += event.times;
    }
  }
}

void Dispatcher::ReportCoalescedEvent(const CoalescedTimerEvent& event) {
  for (auto&& per_sys : pending_events_) {
    auto&& e = per_sys.events;
    std::scoped_lock _(e->lock);
    auto&& key = std::pair(event.key, event.tags);
    auto iter = e->timer_events.find(key);
    if (iter == e->timer_events.end()) {
      e->timer_events[key] = event;
    } else {
      auto&& entry = iter->second;
      FLARE_CHECK(entry.unit == entry.unit);  // How can the unit change?

      // FIXME: This is slow.
      std::map<std::chrono::nanoseconds, std::uint64_t> merged;
      for (auto&& [k, v] : entry.times) {
        merged[k] += v;
      }
      for (auto&& [k, v] : event.times) {
        merged[k] += v;
      }
      entry.times.clear();
      for (auto&& [k, v] : merged) {
        entry.times.emplace_back(k, v);
      }
    }
  }
}

void Dispatcher::AddMonitoringSystem(std::string name,
                                     MonitoringSystem* system) {
  FLARE_LOG_INFO("Enabled monitoring system [{}].", name);
  pending_events_.emplace_back(
      PerSystemEvents{.name = name,
                      .system = system,
                      .events = std::make_unique<PendingEvents>(),
                      .remap_keys = false});
}

void Dispatcher::AddMonitoringSystem(
    std::string name, MonitoringSystem* system,
    internal::HashMap<std::string, std::string> key_mapping,
    bool drop_unknown_keys) {
  FLARE_LOG_INFO("Enabled monitoring system [{}] (with keys remapped).", name);
  pending_events_.emplace_back(
      PerSystemEvents{.name = name,
                      .system = system,
                      .events = std::make_unique<PendingEvents>(),
                      .remap_keys = true,
                      .key_mapping = std::move(key_mapping),
                      .drop_unknown_keys = drop_unknown_keys});
}

void Dispatcher::Start() {
  std::chrono::nanoseconds interval = 10000s;  // ...

  // Use lowest interval among the enabled monitoring systems.
  for (auto&& e : pending_events_) {
    interval =
        std::min(e.system->GetPersonality().minimum_report_interval, interval);
  }

  // But if its too low, respect our own limit.
  interval = std::max<std::chrono::nanoseconds>(
      interval, FLAGS_flare_monitoring_minimum_report_interval * 1ms);
  timer_id_ = internal::TimeKeeper::Instance()->AddTimer(
      ReadCoarseSteadyClock(), interval, [this](auto) { OnTimerProc(); }, true);
}

void Dispatcher::Stop() {
  internal::TimeKeeper::Instance()->KillTimer(timer_id_);
}

void Dispatcher::Join() {
  // NOTHING.
}

void Dispatcher::OnTimerProc() {
  for (auto&& per_sys_env : pending_events_) {
    auto&& events = per_sys_env.events;
    MonitoringSystem::EventBuffers buffer;

    {
      std::scoped_lock _(events->lock);
      if (events->next_flush_at > ReadCoarseSteadyClock()) {
        continue;
      }
      events->next_flush_at =
          ReadCoarseSteadyClock() +
          per_sys_env.system->GetPersonality().minimum_report_interval;

      // Move reports to `buffer`.
      buffer.discrete_events.reserve(events->discrete_events.size());
      for (auto&& e : events->discrete_events) {
        if (auto opt = TryRemapKey(per_sys_env, e.GetKey())) {
          buffer.discrete_events.push_back(std::move(e));
          buffer.discrete_events.back().key = *opt;
        }  // Ignored otherwise.
      }
      events->discrete_events.clear();

      // Key remapping is done by `MoveReports`.
      MoveReports(per_sys_env, &buffer.counter_events, &events->counter_events);
      events->counter_events.clear();
      MoveReports(per_sys_env, &buffer.gauge_events, &events->gauge_events);
      events->gauge_events.clear();
      MoveReports(per_sys_env, &buffer.timer_events, &events->timer_events);
      events->timer_events.clear();
    }

    // Don't call monitoring system provider if we have nothing to report.
    if (!buffer.discrete_events.empty() || !buffer.counter_events.empty() ||
        !buffer.gauge_events.empty() || !buffer.timer_events.empty()) {
      per_sys_env.system->Report(buffer);  // Report events with lock released.
    }
  }
}

}  // namespace flare::monitoring
