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

#include <algorithm>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "gflags/gflags.h"
#include "gmock/gmock-matchers.h"
#include "gtest/gtest.h"

#include "flare/base/down_cast.h"
#include "flare/base/monitoring.h"
#include "flare/base/monitoring/monitoring_system.h"
#include "flare/init/override_flag.h"
#include "flare/testing/main.h"

using namespace std::literals;

FLARE_OVERRIDE_FLAG(flare_monitoring_system,
                    "fancy_sys, fancy_sys2, fancy_sys3");
FLARE_OVERRIDE_FLAG(
    flare_monitoring_key_remap,
    "fancy_sys2=conf/remap_sys2.yaml,fancy_sys3=conf/remap_sys3.yaml");

namespace flare::monitoring {

// Our fancy monitoring system.
class FancyMonitoringSystem : public MonitoringSystem {
 public:
  const Personality& GetPersonality() const override {
    static Personality personality = {};
    return personality;
  }

  void Report(const EventBuffers& events) override {
    std::scoped_lock _(lock_);
    for (auto&& e : events.discrete_events) {
      reported_events_[std::string(e.GetKey())].push_back(e);
    }
    for (auto&& event : events.counter_events) {
      reported_counters_[event.key].push_back(event);
    }
    for (auto&& event : events.gauge_events) {
      reported_gauges_[event.key].push_back(event);
    }
    for (auto&& event : events.timer_events) {
      reported_timers_[event.key].push_back(event);
    }
  }

  auto GetReportedEvents(const std::string& key) const {
    std::scoped_lock _(lock_);
    return reported_events_.at(key);
  }

  auto GetReportedCounters(const std::string& key) const {
    std::scoped_lock _(lock_);
    std::uint64_t sum = 0, times = 0;
    for (auto&& e : reported_counters_.at(key)) {
      sum += e.sum;
      times += e.times;
    }
    return std::pair(sum, times);
  }

  auto GetReportedGauges(const std::string& key) const {
    std::scoped_lock _(lock_);
    std::int64_t sum = 0, times = 0;
    for (auto&& e : reported_gauges_.at(key)) {
      sum += e.sum;
      times += e.times;
    }
    return std::pair(sum, times);
  }

  auto GetReportedTimers(const std::string& key) const {
    std::scoped_lock _(lock_);
    std::chrono::nanoseconds min{std::chrono::nanoseconds::max()},
        max{std::chrono::nanoseconds::min()}, sum{};
    std::uint64_t times = 0;
    for (auto&& e : reported_timers_.at(key)) {
      for (auto&& [k, v] : e.times) {
        min = std::min(min, k);
        max = std::max(max, k);
        sum += k * v;
        times += v;
      }
    }
    return std::tuple(min, max, sum, times);
  }

 private:  // For testing purpose.
  mutable std::mutex lock_;
  std::unordered_map<std::string, std::vector<Event>> reported_events_;
  std::unordered_map<std::string, std::vector<CoalescedCounterEvent>>
      reported_counters_;
  std::unordered_map<std::string, std::vector<CoalescedGaugeEvent>>
      reported_gauges_;
  std::unordered_map<std::string, std::vector<CoalescedTimerEvent>>
      reported_timers_;
};

FLARE_MONITORING_REGISTER_MONITORING_SYSTEM("fancy_sys", FancyMonitoringSystem);
FLARE_MONITORING_REGISTER_MONITORING_SYSTEM("fancy_sys2",
                                            FancyMonitoringSystem);
FLARE_MONITORING_REGISTER_MONITORING_SYSTEM("fancy_sys3",
                                            FancyMonitoringSystem);

MonitoredCounter counter1("fancy-counter");
MonitoredGauge gauge1("fancy-gauge");
MonitoredTimer timer1("fancy-timer");

TEST(MonitoringSystem, All) {
  Report("my fancy key1", 1234, {{"tag1", "v1"}, {"tag2", "v2"}});

  counter1.Add(10);
  gauge1.Add(5);
  gauge1.Subtract(4);
  gauge1.Increment();
  timer1.Report(1s);

  std::this_thread::sleep_for(2s);  // Wait for background timer to flush the
                                    // queued events.

  // Triggers reporting.
  counter1.Increment();
  gauge1.Decrement();
  timer1.Report(2000ms);

  std::this_thread::sleep_for(1s);  // Wait for DPC.

  auto sys1 = down_cast<FancyMonitoringSystem>(
      monitoring_system_registry.TryGet("fancy_sys"));
  auto sys2 = down_cast<FancyMonitoringSystem>(
      monitoring_system_registry.TryGet("fancy_sys2"));

  {
    auto events = sys1->GetReportedEvents("my fancy key1");
    auto events2 = sys2->GetReportedEvents("my fancy key1");
    for (auto&& event : {&events, &events2}) {
      auto&& e = event->front();
      ASSERT_EQ("my fancy key1", e.GetKey());
      ASSERT_EQ(1234, e.value);
      ASSERT_THAT(e.tags, ::testing::ElementsAre(std::pair("tag1", "v1"),
                                                 std::pair("tag2", "v2")));
    }
  }

  {
    auto counter1 = sys1->GetReportedCounters("fancy-counter");
    auto counter2 = sys2->GetReportedCounters("fancy-counter");
    auto gauge1 = sys1->GetReportedGauges("fancy-gauge");
    auto gauge2 = sys2->GetReportedGauges("fancy-gauge");
    auto timer1 = sys1->GetReportedTimers("fancy-timer");
    auto timer2 = sys2->GetReportedTimers("fancy-timer");

    ASSERT_EQ(counter1, counter2);
    ASSERT_EQ(gauge1, gauge2);
    ASSERT_EQ(timer1, timer2);

    ASSERT_EQ(11, counter1.first);
    ASSERT_EQ(2, counter1.second);
    ASSERT_EQ(1, gauge1.first);
    ASSERT_EQ(4, gauge1.second);
    ASSERT_EQ(1s, std::get<0>(timer1));
    ASSERT_EQ(2s, std::get<1>(timer1));
    ASSERT_EQ(3s, std::get<2>(timer1));
    ASSERT_EQ(2, std::get<3>(timer1));
  }

  // Test remap.
  auto sys3 = down_cast<FancyMonitoringSystem>(
      monitoring_system_registry.TryGet("fancy_sys3"));
  auto counter = sys3->GetReportedCounters("not-so-fancy-counter");
  ASSERT_EQ(11, counter.first);
  ASSERT_EQ(2, counter.second);
}

}  // namespace flare::monitoring

FLARE_TEST_MAIN
