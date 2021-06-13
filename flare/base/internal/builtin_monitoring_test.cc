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

#include "flare/base/internal/builtin_monitoring.h"

#include <chrono>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

#include "flare/base/down_cast.h"
#include "flare/base/monitoring/monitoring_system.h"
#include "flare/init/override_flag.h"
#include "flare/testing/main.h"

using namespace std::literals;

FLARE_OVERRIDE_FLAG(flare_monitoring_system, "fancy_sys");
FLARE_OVERRIDE_FLAG(flare_monitoring_builtin_key_mapping,
                    "conf/builtin_key_mapping.yaml");

namespace flare::internal {

// Our fancy monitoring system.
class FancyMonitoringSystem : public monitoring::MonitoringSystem {
 public:
  const Personality& GetPersonality() const  override {
    static Personality personality = {};
    return personality;
  }

  void Report(const EventBuffers& events) override {
    for (auto&& e : events.counter_events) {
      // @sa: `builtin_key_mapping.yaml`
      FLARE_CHECK_EQ("my-builtin-counter", e.key);
      total_counter += e.sum;
    }

    for (auto&& e : events.gauge_events) {
      FLARE_CHECK_EQ("my-builtin-gauge", e.key);
      total_gauge += e.sum;
    }

    for (auto&& timer : events.timer_events) {
      FLARE_CHECK_EQ("my-builtin-timer", timer.key);
      for (auto&& e : timer.times) {
        total_timer = total_timer.load() + e.first * e.second;
      }
    }
  }

  std::atomic<int> total_counter = 0;
  std::atomic<int> total_gauge = 0;
  std::atomic<std::chrono::nanoseconds> total_timer{};
};

FLARE_MONITORING_REGISTER_MONITORING_SYSTEM("fancy_sys", FancyMonitoringSystem);

// Initialized before monitoring system is initialized.
BuiltinMonitoredCounter counter_builtin("counter_builtin");
BuiltinMonitoredCounter counter_builtin2("counter_builtin_not_enabled");

TEST(BuiltinMonitoring, All) {
  BuiltinMonitoredGauge gauge_builtin("gauge_builtin");
  BuiltinMonitoredGauge gauge_builtin2("gauge_builtin_not_enabled");
  BuiltinMonitoredTimer timer_builtin("timer_builtin");
  BuiltinMonitoredTimer timer_builtin2("timer_builtin_not_enabled");

  counter_builtin.Add(1);
  counter_builtin.Increment();
  counter_builtin2.Add(1);
  counter_builtin2.Increment();

  gauge_builtin.Add(1);
  gauge_builtin.Increment();
  gauge_builtin.Subtract(2);
  gauge_builtin.Decrement();
  gauge_builtin.Decrement();

  gauge_builtin2.Add(1);
  gauge_builtin2.Increment();
  gauge_builtin2.Subtract(2);
  gauge_builtin2.Decrement();
  gauge_builtin2.Decrement();

  timer_builtin.Report(10ms);
  timer_builtin2.Report(10ms);

  std::this_thread::sleep_for(2s);

  // Trigger report.
  counter_builtin.Add(0);
  counter_builtin2.Add(0);
  gauge_builtin.Add(0);
  gauge_builtin2.Add(0);
  timer_builtin.Report(0ns);
  timer_builtin2.Report(0ns);

  std::this_thread::sleep_for(2s);  // Wait for DPC..

  auto sys = flare::down_cast<FancyMonitoringSystem>(
      monitoring::monitoring_system_registry.TryGet("fancy_sys"));

  EXPECT_EQ(2, sys->total_counter.load());
  EXPECT_EQ(-2, sys->total_gauge.load());
  EXPECT_EQ(10ms, sys->total_timer.load());
}

}  // namespace flare::internal

FLARE_TEST_MAIN
