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

#include "flare/base/monitoring.h"

#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include "gflags/gflags.h"
#include "googletest/gmock/gmock-matchers.h"
#include "googletest/gtest/gtest.h"

#include "flare/base/down_cast.h"
#include "flare/base/monitoring/monitoring_system.h"
#include "flare/base/random.h"
#include "flare/fiber/this_fiber.h"
#include "flare/init/override_flag.h"
#include "flare/testing/main.h"

using namespace std::literals;

FLARE_OVERRIDE_FLAG(flare_monitoring_system, "fancy_sys");
FLARE_OVERRIDE_FLAG(flare_monitoring_minimum_report_interval, 10);

namespace flare::monitoring {

// Our fancy monitoring system.
class FancyMonitoringSystem : public MonitoringSystem {
 public:
  void SetDumb(bool f) { is_dumb_ = f; }

  const Personality& GetPersonality() const override {
    static const Personality personality = {.minimum_report_interval = 0s};
    return personality;
  }

  void Report(const EventBuffers& events) override {
    if (is_dumb_.load(std::memory_order_relaxed)) {
      return;
    }
    std::scoped_lock _(lock_);

    reported_events_.insert(reported_events_.end(),
                            events.discrete_events.begin(),
                            events.discrete_events.end());
    for (auto&& e : events.counter_events) {
      if (e.tags.empty()) {
        counters_[e.key] += e.sum;
      } else {
        counters_tagged_[e.key][NormalizeTags(e.tags)] += e.sum;
      }
    }
    for (auto&& e : events.gauge_events) {
      if (e.tags.empty()) {
        gauges_[e.key] += e.sum;
      } else {
        gauges_tagged_[e.key][NormalizeTags(e.tags)] += e.sum;
      }
    }
    for (auto&& e : events.timer_events) {
      for (auto&& [k, v] : e.times) {
        if (e.tags.empty()) {
          timers_[e.key][k] += v;
        } else {
          timers_tagged_[e.key][NormalizeTags(e.tags)][k] += v;
        }
      }
    }
  }

  auto GetReportedEvents() const {
    std::scoped_lock _(lock_);
    return reported_events_;
  }

  auto GetReportedCounter(
      const std::string& key,
      const std::vector<std::pair<std::string, std::string>>& tags = {}) const {
    std::scoped_lock _(lock_);
    if (tags.empty()) {
      return counters_.at(key);
    } else {
      return counters_tagged_.at(key).at(NormalizeTags(tags));
    }
  }

  auto GetReportedGauge(
      const std::string& key,
      const std::vector<std::pair<std::string, std::string>>& tags = {}) const {
    std::scoped_lock _(lock_);
    if (tags.empty()) {
      return gauges_.at(key);
    } else {
      return gauges_tagged_.at(key).at(NormalizeTags(tags));
    }
  }

  auto GetReportedTimer(
      const std::string& key,
      const std::vector<std::pair<std::string, std::string>>& tags = {}) const {
    std::scoped_lock _(lock_);
    const std::map<std::chrono::nanoseconds, std::size_t>* ptimers;
    if (tags.empty()) {
      ptimers = &timers_.at(key);
    } else {
      ptimers = &timers_tagged_.at(key).at(NormalizeTags(tags));
    }
    std::chrono::nanoseconds sum = 0ns;
    std::size_t times = 0;
    for (auto&& [k, v] : *ptimers) {
      sum += k * v;
      times += v;
    }
    return sum / times;
  }

  bool IsCounterReported(const std::string& key) {
    std::scoped_lock _(lock_);
    return counters_.count(key) != 0;
  }

  bool IsGaugeReported(const std::string& key) {
    std::scoped_lock _(lock_);
    return gauges_.count(key) != 0;
  }

  bool IsTimerReported(const std::string& key) {
    std::scoped_lock _(lock_);
    return timers_.count(key) != 0;
  }

  static std::vector<std::pair<std::string, std::string>> NormalizeTags(
      std::vector<std::pair<std::string, std::string>> tags) {
    std::sort(tags.begin(), tags.end());
    return tags;
  }

 private:  // For testing purpose.
  std::atomic<bool> is_dumb_{};
  mutable std::mutex lock_;
  std::vector<Event> reported_events_;
  std::map<std::string, std::uint64_t> counters_;
  std::map<std::string, std::int64_t> gauges_;
  std::map<std::string, std::map<std::chrono::nanoseconds, std::uint64_t>>
      timers_;
  std::map<std::string,
           internal::HashMap<std::vector<std::pair<std::string, std::string>>,
                             std::uint64_t>>
      counters_tagged_;
  std::map<std::string,
           internal::HashMap<std::vector<std::pair<std::string, std::string>>,
                             std::int64_t>>
      gauges_tagged_;
  std::map<std::string,
           internal::HashMap<std::vector<std::pair<std::string, std::string>>,
                             std::map<std::chrono::nanoseconds, std::uint64_t>>>
      timers_tagged_;
};

FLARE_MONITORING_REGISTER_MONITORING_SYSTEM("fancy_sys", FancyMonitoringSystem);

TEST(Monitoring, OutOfDutyFlush) {
  // Don't collect the events to speed things up.
  auto monitoring_sys = down_cast<FancyMonitoringSystem>(
      monitoring_system_registry.TryGet("fancy_sys"));

  MonitoredTimer timer1("another-timer", 1ns);
  timer1.Report(1s);

  // Thread-locally buffered reports has not been flushed yet.
  ASSERT_FALSE(monitoring_sys->IsTimerReported("another-timer"));

  std::this_thread::sleep_for(200ms);
  this_fiber::Yield();                 // Triggers "out-of-duty" callback.
  std::this_thread::sleep_for(100ms);  // Wait for DPC to run.
  ASSERT_EQ(1s, monitoring_sys->GetReportedTimer("another-timer"));
}

MonitoredCounter counter1("fancy-counter1");
MonitoredCounter counter2("fancy-counter2");
MonitoredGauge gauge1("fancy-gauge");
MonitoredTimer timer1("fancy-timer");

TEST(Monitoring, Basics) {
  Report("my fancy key1", 1234, {{"tag1", "v1"}, {"tag2", "v2"}});
  Report(Reading::Newest, "my fancy key2", 1235,
         {{"tag3", "v3"}, {"tag4", "v4"}});

  counter1.Add(1);
  counter2.Add(1);
  gauge1.Add(1);
  gauge1.Subtract(1);
  gauge1.Increment();
  timer1.Report(1s);

  std::this_thread::sleep_for(2s);  // Wait for background timer to flush the
                                    // queued events.

  counter2.Add(10);
  counter1.Increment();
  gauge1.Decrement();
  timer1.Report(3s);

  std::this_thread::sleep_for(1s);  // Wait for DPC.

  auto sys = down_cast<FancyMonitoringSystem>(
      monitoring_system_registry.TryGet("fancy_sys"));
  auto events = sys->GetReportedEvents();
  ASSERT_EQ(2, events.size());
  ASSERT_EQ("my fancy key1", events[0].GetKey());
  ASSERT_EQ("my fancy key2", events[1].GetKey());
  ASSERT_EQ(1234, events[0].value);
  ASSERT_EQ(1235, events[1].value);

  ASSERT_THAT(events[0].tags, ::testing::ElementsAre(std::pair("tag1", "v1"),
                                                     std::pair("tag2", "v2")));
  ASSERT_THAT(events[1].tags, ::testing::ElementsAre(std::pair("tag3", "v3"),
                                                     std::pair("tag4", "v4")));

  ASSERT_EQ(2, sys->GetReportedCounter("fancy-counter1"));
  ASSERT_EQ(11, sys->GetReportedCounter("fancy-counter2"));
  ASSERT_EQ(0, sys->GetReportedGauge("fancy-gauge"));
  ASSERT_EQ(2s, sys->GetReportedTimer("fancy-timer"));

  Report("my fancy key1", 1234, {{"tag1", "v1"}, {"tag2", "v2"}});
  Report(Reading::Newest, "my fancy key2", 1235,
         {{"tag3", "v3"}, {"tag4", "v4"}});

  std::this_thread::sleep_for(1s);  // Wait for flushing internal buffers.
  events = sys->GetReportedEvents();
  ASSERT_EQ(4, events.size());
}

TEST(Monitoring, TaggedReport) {
  MonitoredCounter counter1("tagged-counter1", {{"key1", "value1"}});
  MonitoredCounter counter2("tagged-counter2",
                            {{"key2", "value2"}, {"key2-a", "value2-a"}});
  MonitoredGauge gauge1("tagged-gauge", {{"key", "value"}});
  MonitoredTimer timer1("tagged-timer", 1ns, {{"key", "value"}});
  MonitoredTimer timer2("tagged-timer2", 1us, {{"key", "value"}});
  MonitoredTimer timer3("timer3", 1ms);
  counter1.Add(1);
  gauge1.Increment();
  timer1.Report(1s);

  counter1.Add(1, {{"set", "1"}});
  counter1.Add(1, {{"set", "1"}});
  counter2.Add(1, {{"set", "1"}});
  gauge1.Add(1, {{"set", "1"}});
  gauge1.Subtract(1, {{"set", "1"}});
  timer1.Report(1s, {{"set", "1"}});
  timer1.Report(1s, {{"set", "1"}});
  timer2.Report(1s);
  timer3.Report(99ms, {{"tag", "value"}});
  timer3.Report(101ms, {{"tag", "value"}});

  counter1.Add(1, {{"set", "2"}});

  std::this_thread::sleep_for(2s);

  // Trigger report.
  counter1.Add(0);
  counter2.Add(0);
  gauge1.Add(0);
  timer1.Report(0ns);
  timer2.Report(4s);
  timer3.Report(100ms, {{"tag", "value"}});

  std::this_thread::sleep_for(1s);  // Wait for DPC..

  auto sys = down_cast<FancyMonitoringSystem>(
      monitoring_system_registry.TryGet("fancy_sys"));
  auto events = sys->GetReportedEvents();

  ASSERT_EQ(1,
            sys->GetReportedCounter("tagged-counter1", {{"key1", "value1"}}));
  ASSERT_EQ(1, sys->GetReportedGauge("tagged-gauge", {{"key", "value"}}));
  ASSERT_EQ(500ms, sys->GetReportedTimer("tagged-timer", {{"key", "value"}}));

  ASSERT_EQ(2, sys->GetReportedCounter("tagged-counter1",
                                       {{"key1", "value1"}, {"set", "1"}}));
  ASSERT_EQ(0, sys->GetReportedGauge("tagged-gauge",
                                     {{"key", "value"}, {"set", "1"}}));
  ASSERT_EQ(1s, sys->GetReportedTimer("tagged-timer",
                                      {{"key", "value"}, {"set", "1"}}));
  ASSERT_EQ(2500ms, sys->GetReportedTimer("tagged-timer2", {{"key", "value"}}));
  ASSERT_EQ(100ms, sys->GetReportedTimer("timer3", {{"tag", "value"}}));

  ASSERT_EQ(1, sys->GetReportedCounter("tagged-counter1",
                                       {{"key1", "value1"}, {"set", "2"}}));
}

TEST(Monitoring, MultipleTags) {
  // Don't collect the events to speed things up.
  down_cast<FancyMonitoringSystem>(
      monitoring_system_registry.TryGet("fancy_sys"))
      ->SetDumb(true);

  MonitoredTimer timer1("tagged-timer", 1ns, {{"key", "value"}});
  MonitoredTimer timer2("tagged-timer2", 1us, {{"key", "value"}});

  std::thread ts[50];
  std::vector<std::string> random_tag_keys;
  std::vector<std::string> random_tag_values;

  for (int i = 0; i != 20; ++i) {
    random_tag_keys.push_back(std::to_string(Random(1234567)));
    random_tag_values.push_back(std::to_string(Random(1234567)));
  }

  std::sort(random_tag_keys.begin(), random_tag_keys.end());
  random_tag_keys.erase(
      std::unique(random_tag_keys.begin(), random_tag_keys.end()),
      random_tag_keys.end());
  std::sort(random_tag_values.begin(), random_tag_values.end());
  random_tag_values.erase(
      std::unique(random_tag_values.begin(), random_tag_values.end()),
      random_tag_values.end());

  auto&& get_key = [&](int i) -> decltype(auto) {
    return random_tag_keys[i % random_tag_keys.size()];
  };
  auto&& get_value = [&](int i) -> decltype(auto) {
    return random_tag_values[i % random_tag_values.size()];
  };

  for (auto&& e : ts) {
    e = std::thread([&, start = ReadSteadyClock()] {
      int reports = 0;
      while (ReadSteadyClock() - start < 10s) {
        auto delay = Random(1234);
        auto x = Random(123), y = Random(123);
        for (int i = 0; i != 10000; ++i) {
          x += i;
          y += i;

          timer1.Report(delay * 1ns, {{get_key(x), get_value(y)}});
          timer2.Report(delay * 1ns, {{get_key(x), get_value(y)}});

          timer1.Report(delay * 1ns, {{get_key(x), get_value(y)},
                                      {get_key(x + 1), get_value(y + 1)}});
          timer2.Report(delay * 1ns, {{get_key(x), get_value(y)},
                                      {get_key(x + 1), get_value(y + 1)}});

          timer1.Report(delay * 1ns, {{get_key(x), get_value(y)},
                                      {get_key(x + 1), get_value(y + 1)},
                                      {get_key(x + 2), get_value(y + 2)}});
          timer2.Report(delay * 1ns, {{get_key(x), get_value(y)},
                                      {get_key(x + 1), get_value(y + 1)},
                                      {get_key(x + 2), get_value(y + 2)}});

          timer1.Report(delay * 1ns, {{get_key(x), get_value(y)},
                                      {get_key(x + 1), get_value(y + 1)},
                                      {get_key(x + 2), get_value(y + 2)},
                                      {get_key(x + 3), get_value(y + 3)}});
          timer2.Report(delay * 1ns, {{get_key(x), get_value(y)},
                                      {get_key(x + 1), get_value(y + 1)},
                                      {get_key(x + 2), get_value(y + 2)},
                                      {get_key(x + 3), get_value(y + 3)}});
          reports += 8;
        }
      }

      // The result doesn't look quite promising, there are simply too many
      // tags.
      FLARE_LOG_INFO("Average reporting cost: {} ns per event.",
                     (1s / 1ns) / (reports / 10));
    });
  }

  for (auto&& e : ts) {
    e.join();
  }
}

}  // namespace flare::monitoring

FLARE_TEST_MAIN
