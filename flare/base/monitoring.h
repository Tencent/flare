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

#ifndef FLARE_BASE_MONITORING_H_
#define FLARE_BASE_MONITORING_H_

#include <algorithm>
#include <chrono>
#include <initializer_list>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "flare/base/chrono.h"
#include "flare/base/internal/hash_map.h"
#include "flare/base/monitoring/event.h"
#include "flare/base/monitoring/fwd.h"
#include "flare/base/thread/thread_local.h"

// Please note that not all monitoring systems support every operations here.
// You will see an error log in case you're reporting values in an unsupported
// way.

namespace flare {

// This class allows you to report a value in `ExportedXxx` way.
//
// The value being reported is treated as a "Counter" (using the definition of
// Prometheus, @sa: https://prometheus.io/docs/concepts/metric_types/#counter.)
class MonitoredCounter {
 public:
  // Constructs an instance of monitored counter.
  //
  // `extra_tags` are always carried with each coalesced report. If the
  // monitoring system being used does not support tags, they're dropped
  // silently (presumably with a warning printed.).
  //
  // Note that instantiating this class is slow. You should avoid keep
  // instantiating it. (Reporting values via this class is fast, though.)
  explicit MonitoredCounter(
      std::string key,
      std::vector<std::pair<std::string, std::string>> extra_tags = {});
  ~MonitoredCounter();

  // Add a value to the counter.
  //
  // We don't support carrying tags when reporting via this class. If you want
  // to carry tags with your report, use `monitoring::Report(...)` instead.
  void Add(std::uint64_t value) noexcept;

  // Shorthand for `Add(1)`.
  void Increment() noexcept;

  // Some monitoring systems allows you to attach your tag with each reported
  // value. In case you need this functionality, we provide this method for you.
  //
  // Note that this overload is considerably slower than the one without tag.
  void Add(std::uint64_t value,
           std::initializer_list<std::pair<std::string_view, std::string_view>>
               tags);
  void Increment(
      std::initializer_list<std::pair<std::string_view, std::string_view>>
          tags);

 private:
  struct State;

  void FlushBufferCheck(State* state) noexcept;
  void FlushBufferedReports() noexcept;

 private:
  std::string key_;
  std::vector<std::pair<std::string, std::string>> extra_tags_;
  std::uint64_t out_of_duty_registration_;

  internal::ThreadLocalAlwaysInitialized<State> state_;
};

// The value being reported is treated as a "Gauge".
class MonitoredGauge {
 public:
  explicit MonitoredGauge(
      std::string key,
      std::vector<std::pair<std::string, std::string>> extra_tags = {});
  ~MonitoredGauge();

  // Add a value to the gauge.
  void Add(std::int64_t value) noexcept;

  // Subtract a value from the gauge.
  void Subtract(std::int64_t value) noexcept;

  // Shorthand for `Add(1)` and `Subtract(1)`.
  void Increment() noexcept;
  void Decrement() noexcept;

  // @sa: `MonitoredCounter::Add(...)`. The same argument applies here.
  void Add(std::int64_t value,
           std::initializer_list<std::pair<std::string_view, std::string_view>>
               tags);
  void Subtract(
      std::int64_t value,
      std::initializer_list<std::pair<std::string_view, std::string_view>>
          tags);
  void Increment(
      std::initializer_list<std::pair<std::string_view, std::string_view>>
          tags);
  void Decrement(
      std::initializer_list<std::pair<std::string_view, std::string_view>>
          tags);

 private:
  struct State;

  void Report(std::int64_t value) noexcept;
  void Report(
      std::int64_t value,
      std::initializer_list<std::pair<std::string_view, std::string_view>>
          tags) noexcept;

  void FlushBufferCheck(State* state) noexcept;
  void FlushBufferedReports() noexcept;

 private:
  std::string key_;
  std::vector<std::pair<std::string, std::string>> extra_tags_;
  std::uint64_t out_of_duty_registration_;

  internal::ThreadLocalAlwaysInitialized<State> state_;
};

// The value being reported as a timer. This is usually used to report operation
// latencies.
//
// I do think this name ("Timer") is misleading, but I haven't come up with a
// better name yet.
class MonitoredTimer {
 public:
  // By default time unit for the `Timer`is 1us. You can override this behavior
  // with overload below.
  explicit MonitoredTimer(
      std::string key,
      std::vector<std::pair<std::string, std::string>> extra_tags = {});

  // For monitoring systems that are aware of time duration (rather than plain
  // integers), `UNIT` SPECIFIED HERE MIGHT GET IGNORED. Here `unit` is mainly
  // used by those "legacy" monitoring systems who only understand integral
  // numbers. To report to those monitoring systems, we divide the durations by
  // `unit` before actually reporting them.
  MonitoredTimer(
      std::string key, std::chrono::nanoseconds unit,
      std::vector<std::pair<std::string, std::string>> extra_tags = {});

  ~MonitoredTimer();

  // Report a duration.
  void Report(std::chrono::nanoseconds duration) noexcept;

  // @sa: `MonitoredCounter::Add(...)`. The same argument applies here.
  void Report(
      std::chrono::nanoseconds duration,
      std::initializer_list<std::pair<std::string_view, std::string_view>>
          tags);

 private:
  // For reports with less than this value, they're stored in a more optimized
  // way.
  inline static constexpr auto kOptimizedForDurationThreshold = 100;
  struct State;

  void FlushBufferCheck(State* state) noexcept;
  void FlushBufferedReports() noexcept;

 private:
  std::string key_;
  std::chrono::nanoseconds unit_;
  std::vector<std::pair<std::string, std::string>> extra_tags_;
  std::uint64_t (*as_count_)(MonitoredTimer*,
                             std::chrono::nanoseconds duration);
  std::uint64_t out_of_duty_registration_;

  internal::ThreadLocalAlwaysInitialized<State> state_;
};

// Frankly I do think `metrics` looks better than `monitoring`. However, they're
// not exactly the same. A monitoring system do more than what a metric system
// do, notably the former supports sending alarms to operator directly from the
// program. Besides, naming it as "monitoring" makes it more clear that values
// reported here are "monitored" by someone (as opposed to those exported via
// `ExportedXxx`, which is more often than not used in debugging.)
namespace monitoring {

// Report a monitored value.
//
// Certain monitoring systems allow you to provide extra information (in
// KV-pairs) for latter inspection. These KV-pairs can be provided via parameter
// `extra`. Note that you'd incur a perf. penalty if non-empty `extra` is
// provided.
void Report(Reading reading, std::string_view key,
            std::uint64_t value,  // Do we support `double`?
            std::initializer_list<std::pair<std::string_view, std::string_view>>
                tags = {});

// Same as `Report()` above, except that you don't have to provide `Reading` if
// the provider you're using can infer it implicitly.
//
// Not all providers (notably ZhiYan does not) support this "generic" report
// facility.
inline void Report(
    std::string_view key, std::uint64_t value,
    std::initializer_list<std::pair<std::string_view, std::string_view>> tags =
        {}) {
  return Report(Reading::Inferred, key, value, tags);
}

// Report a string, often it's an alert message to be sent to operator.
//
// I'm not aware of any monitoring system we're using support this, so let's
// defer our implementation.
//
// void Report(std::string_view key, std::string value);

}  // namespace monitoring

///////////////////////////////////////
// Implementation goes below.        //
///////////////////////////////////////

namespace monitoring::detail {

// Hash specialized for tags. Element order in tags is not significant.
struct UnorderedTagHash {
  template <class T>
  std::size_t operator()(const T& values) const noexcept {
    std::size_t h = 0;
    for (auto&& e : values) {
      h ^= internal::Hash<>{}(e);
    }
    return h;
  }

  std::size_t operator()(const ComparableTags& values) const noexcept {
    return operator()(values.GetTags());
  }
};

}  // namespace monitoring::detail

// `MonitoredCounter`

struct MonitoredCounter::State {
  struct CoalescedReports {
    std::uint64_t sum = 0;
    std::uint64_t times = 0;

    void Clear() noexcept {
      sum = 0;
      times = 0;
    }
  };

  std::chrono::steady_clock::time_point next_report{};
  bool dirty = false;  // Cleared on `Flush`, set on `Report()`.
  CoalescedReports fast_reports;
  internal::HashMap<monitoring::ComparableTags, CoalescedReports,
                    monitoring::detail::UnorderedTagHash>
      tagged_reports;
};

inline void MonitoredCounter::Add(std::uint64_t value) noexcept {
  FLARE_CHECK_GE(value, 0);
  auto&& state = state_.Get();
  state->dirty = true;
  state->fast_reports.sum += value;
  ++state->fast_reports.times;
  FlushBufferCheck(state);
}

inline void MonitoredCounter::Increment() noexcept { Add(1); }

inline void MonitoredCounter::FlushBufferCheck(State* state) noexcept {
  if (auto now = ReadCoarseSteadyClock();
      FLARE_UNLIKELY(state->next_report <= now)) {
    FlushBufferedReports();
  }
}

// `MonitoredGauge`

struct MonitoredGauge::State {
  struct CoalescedReports {
    std::uint64_t sum = 0;
    std::uint64_t times = 0;

    void Clear() noexcept {
      sum = 0;
      times = 0;
    }
  };

  std::chrono::steady_clock::time_point next_report{};
  bool dirty = false;
  CoalescedReports fast_reports;
  // FIXME: Slow here. Consider using `std::unordered_map` with transparent hash
  // here.
  internal::HashMap<monitoring::ComparableTags, CoalescedReports,
                    monitoring::detail::UnorderedTagHash>
      tagged_reports;
};

inline void MonitoredGauge::Add(std::int64_t value) noexcept {
  FLARE_CHECK_GE(value, 0);
  Report(value);
}

inline void MonitoredGauge::Subtract(std::int64_t value) noexcept {
  FLARE_CHECK_GE(value, 0);
  Report(-value);
}

inline void MonitoredGauge::Report(std::int64_t value) noexcept {
  auto&& state = state_.Get();
  state->dirty = true;
  state->fast_reports.sum += value;
  ++state->fast_reports.times;
  FlushBufferCheck(state);
}

inline void MonitoredGauge::Increment() noexcept { Add(1); }
inline void MonitoredGauge::Decrement() noexcept { Subtract(1); }

inline void MonitoredGauge::FlushBufferCheck(State* state) noexcept {
  if (auto now = ReadCoarseSteadyClock();
      FLARE_UNLIKELY(state->next_report <= now)) {
    FlushBufferedReports();
  }
}

// `MonitoredTimer`

struct MonitoredTimer::State {
  struct CoalescedReports {
    std::size_t fast_times[kOptimizedForDurationThreshold] = {};
    std::unordered_map<std::uint64_t, std::size_t> times;

    void Clear() {
      times.clear();
      memset(&fast_times, 0, sizeof(fast_times));
    }
  };

  std::chrono::steady_clock::time_point next_report{};
  bool dirty = false;
  CoalescedReports fast_reports;
  internal::HashMap<monitoring::ComparableTags, CoalescedReports,
                    monitoring::detail::UnorderedTagHash>
      tagged_reports;
};

inline void MonitoredTimer::Report(std::chrono::nanoseconds duration) noexcept {
  auto&& state = state_.Get();
  state->dirty = true;

  // `FLARE_LIKELY` here? I'm not sure if that makes sense.
  //
  // Note that conversion (as done by `as_count_` can NOT be elide, otherwise
  // given the resolution of `duration`, unless the user is indeed using `1ns`
  // as `unit`, it's unlikely that we can ever coalesce any reports.
  if (auto count = as_count_(this, duration);
      count < kOptimizedForDurationThreshold) {
    ++state->fast_reports.fast_times[count];
  } else {
    ++state->fast_reports.times[count];
  }
  FlushBufferCheck(state);
}

inline void MonitoredTimer::FlushBufferCheck(State* state) noexcept {
  if (auto now = ReadCoarseSteadyClock();
      FLARE_UNLIKELY(state->next_report <= now)) {
    FlushBufferedReports();
  }
}

}  // namespace flare

#endif  // FLARE_BASE_MONITORING_H_
