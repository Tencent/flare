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

// clang-format off
#include "flare/base/monitoring.h"
// clang-format on

#include <chrono>
#include <mutex>
#include <thread>

#include "gtest/gtest.h"

#include "flare/base/down_cast.h"
#include "flare/base/monitoring/monitoring_system.h"
#include "flare/fiber/this_fiber.h"
#include "flare/init/override_flag.h"
#include "flare/testing/main.h"

using namespace std::literals;

FLARE_OVERRIDE_FLAG(flare_monitoring_system, "fancy_sys");
FLARE_OVERRIDE_FLAG(flare_monitoring_minimum_report_interval, 10);
FLARE_OVERRIDE_FLAG(flare_monitoring_extra_tags, "FANCY_ENV=12345");

namespace flare::monitoring {

// Our fancy monitoring system.
class FancyMonitoringSystem : public MonitoringSystem {
 public:
  const Personality& GetPersonality() const override {
    static const Personality personality = {.minimum_report_interval = 0s};
    return personality;
  }

  void Report(const EventBuffers& events) override {
    std::scoped_lock _(lock);
    evs.counter_events.insert(evs.counter_events.begin(),
                              events.counter_events.begin(),
                              events.counter_events.end());
    evs.gauge_events.insert(evs.gauge_events.begin(),
                            events.gauge_events.begin(),
                            events.gauge_events.end());
    evs.timer_events.insert(evs.timer_events.begin(),
                            events.timer_events.begin(),
                            events.timer_events.end());
  }

  EventBuffers GetEvents() {
    std::scoped_lock _(lock);
    return evs;
  }

  std::mutex lock;
  EventBuffers evs;
};

FLARE_MONITORING_REGISTER_MONITORING_SYSTEM("fancy_sys", FancyMonitoringSystem);

TEST(Monitoring, GlobalTag) {
  auto monitoring_sys = down_cast<FancyMonitoringSystem>(
      monitoring_system_registry.TryGet("fancy_sys"));

  MonitoredCounter counter1("fancy-counter1");
  MonitoredGauge gauge1("fancy-gauge");
  MonitoredTimer timer1("fancy-timer");
  counter1.Add(1);
  gauge1.Add(1);
  timer1.Report(1ns);

  auto evs = monitoring_sys->GetEvents();
  while (evs.counter_events.empty() || evs.gauge_events.empty() ||
         evs.timer_events.empty()) {
    std::this_thread::sleep_for(200ms);
    evs = monitoring_sys->GetEvents();
  }

  ASSERT_EQ(1, evs.counter_events[0].tags.size());
  EXPECT_EQ("FANCY_ENV", evs.counter_events[0].tags[0].first);
  EXPECT_EQ("12345", evs.counter_events[0].tags[0].second);

  ASSERT_EQ(1, evs.gauge_events[0].tags.size());
  EXPECT_EQ("FANCY_ENV", evs.gauge_events[0].tags[0].first);
  EXPECT_EQ("12345", evs.gauge_events[0].tags[0].second);

  ASSERT_EQ(1, evs.timer_events[0].tags.size());
  EXPECT_EQ("FANCY_ENV", evs.timer_events[0].tags[0].first);
  EXPECT_EQ("12345", evs.timer_events[0].tags[0].second);
}

}  // namespace flare::monitoring

FLARE_TEST_MAIN
