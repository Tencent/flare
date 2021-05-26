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

#include <atomic>
#include <chrono>
#include <memory>
#include <utility>
#include <vector>

#include "thirdparty/gflags/gflags.h"

#include "flare/base/align.h"
#include "flare/base/deferred.h"
#include "flare/base/exposed_var.h"
#include "flare/base/internal/background_task_host.h"
#include "flare/base/internal/circular_buffer.h"
#include "flare/base/internal/cpu.h"
#include "flare/base/internal/dpc.h"
#include "flare/base/internal/time_keeper.h"
#include "flare/base/logging.h"
#include "flare/base/monitoring/dispatcher.h"
#include "flare/base/monitoring/event.h"
#include "flare/base/monitoring/monitoring_system.h"
#include "flare/base/string.h"
#include "flare/base/thread/out_of_duty_callback.h"
#include "flare/base/thread/thread_local/ref_counted.h"
#include "flare/base/tsc.h"

DEFINE_string(
    flare_monitoring_extra_tags, "",
    "If desired, you may specify tags to be reporting along with every "
    "monitored value. This option is specified as K1=V1;K2=V2;K3=V3;...");

using namespace std::literals;

namespace flare {

namespace monitoring {

namespace {

ExposedMetrics<std::uint64_t> flush_events_delay(
    "flare/monitoring/flush_events_delay");

// Wrapper on `CircularBuffer`, providing some handy functionality required by
// us.
class alignas(hardware_destructive_interference_size) GuardedCircularBuffer
    : public RefCounted<GuardedCircularBuffer> {
  using Buffer = internal::CircularBuffer<Event>;

 public:
  ~GuardedCircularBuffer() {
    while (!AcquireOwnership()) {  // Background task is working on us, waiting
                                   // for them.
      // NOTHING.
    }

    // FIXME: Anything still in `buffer_` is silently dropped.
  }

  Buffer* Get() noexcept { return &buffer_; }

  Deferred AcquireOwnership() noexcept {
    if (!acquired_.exchange(true)) {
      return Deferred{[this] { acquired_.store(false); }};
    }
    return Deferred{};
  }

  int GetNodeId() const noexcept { return node_id_; }

 private:
  // This buffer should be large enough. Even if we can produce so many events,
  // we're unlikely to be able to consume them in time.
  Buffer buffer_{1048576};
  std::atomic<bool> acquired_{false};
  // The object is initialized on first access (by its owning thread,
  // obviously), therefore `node_id_` should be initialized correctly.
  int node_id_ = internal::numa::GetCurrentNode();
};

internal::ThreadLocalRefCounted<GuardedCircularBuffer> pending_events;

void ReportEvents();

// Start background timer to run `ReportEvents` periodically.
class MonitorTimerInitializer {
 public:
  MonitorTimerInitializer() {
    timer_id_ = internal::TimeKeeper::Instance()->AddTimer(
        {}, 100ms, [](auto) { ReportEvents(); }, false);
  }

  ~MonitorTimerInitializer() {
    internal::TimeKeeper::Instance()->KillTimer(timer_id_);
  }

 private:
  std::uint64_t timer_id_;
};

void ReportEvents() {
  pending_events.ForEach([](GuardedCircularBuffer* buffer) {
    auto guard = buffer->AcquireOwnership();
    if (!guard) {
      return;  // Someone else is working on the buffer.
    }

    // An extra ref. to `buffer` is kept so that we won't risk using freed
    // TLS even if the owner thread exited before our callback is called.
    struct CapturedBufferRef {
      // Caution: Order is significant here. If our callback is not run (because
      // the whole program is leaving), `ownership` must be releaesed before
      // destroying `buffer`. Otherwise deadlock can occur.
      RefPtr<GuardedCircularBuffer> buffer;
      flare::Deferred ownership;
    };
    auto cb = [ref = CapturedBufferRef{.buffer = RefPtr(ref_ptr, buffer),
                                       .ownership = std::move(guard)}] {
      thread_local std::vector<Event> events;
      // No need to check if `events`'s capacity is too large (and reset it
      // if it is, as was done in `internal/dpc.cc`) as the `buffer`'s
      // internal storaged is bounded.

      ScopedDeferred _([&] { events.clear(); });
      ref.buffer->Get()->Pop(&events);
      if (!events.empty()) {
        Dispatcher::Instance()->ReportEvents(events);
      }
    };
    internal::BackgroundTaskHost::Instance()->Queue(buffer->GetNodeId(),
                                                    std::move(cb));
  });
}

const std::vector<std::pair<std::string, std::string>>& GetGlobalExtraTags() {
  using KV = std::vector<std::pair<std::string, std::string>>;
  static const flare::NeverDestroyed<KV> tags([] {
    KV result;
    auto split = Split(FLAGS_flare_monitoring_extra_tags, ";");
    for (auto&& e : split) {
      auto kv = Split(e, "=");
      FLARE_CHECK_EQ(2, kv.size(),
                     "Invalid global extra monitoring tag KV-pair: {}", e);
      result.emplace_back(kv[0], kv[1]);
    }
    return result;
  }());
  return *tags;
}

// Initialization of timer is delayed until first call to `Report()` to avoid
// static initialization order fiasco.
void InitializeMonitorTimerOnce() {
  static MonitorTimerInitializer initializer;
  // NOTHING.
}

ComparableTags AsComparableTags(
    std::initializer_list<std::pair<std::string_view, std::string_view>> tags) {
  std::vector<std::pair<std::string, std::string>> values;

  for (auto&& [k, v] : tags) {
    values.emplace_back(k, v);
  }
  return ComparableTags(values);
}

std::vector<std::pair<std::string, std::string>> MergeTags(
    const std::vector<std::pair<std::string, std::string>>& left,
    const std::vector<std::pair<std::string, std::string>>& right) {
  std::unordered_map<std::string, std::string> m;

  for (auto&& [k, v] : left) {
    m[k] = v;
  }
  for (auto&& [k, v] : right) {  // Overwrites what's in `left` on duplicate.
    m[k] = v;
  }

  std::vector<std::pair<std::string, std::string>> merged;
  for (auto&& [k, v] : m) {
    merged.push_back(std::pair(k, v));
  }
  return merged;
}

template <class Monitor, class Extra, class State>
auto SaveReportEssentialsAndClear(const std::string& name, Extra extra,
                                  State* state) {
  ScopedDeferred _([&, start = ReadTsc()] {
    auto cost = DurationFromTsc(start, ReadTsc());
    if (FLARE_UNLIKELY(cost > 5ms)) {
      // TODO(luobogao): We can suppress this warning if we're called in
      // "out-of-duty" callback -- any slow down there shouldn't affect our
      // responsiveness too much.
      FLARE_LOG_WARNING(
          "Flushing monitoring event [{}] of type [{}] cache costs {} ms. Too "
          "many events?",
          name, GetTypeName<Monitor>(), cost / 1ms);
    }
    flush_events_delay->Report(cost / 1us);
  });
  using Reports = std::decay_t<decltype(state->fast_reports)>;

  // TODO(luobogao): For better responsiveness, we should consider using a
  // global object pool for allocating this object. CAUTION: NUMA-aware object
  // pool won't work if CPU affinity of background task host is set without NUMA
  // in mind.
  struct Essentials {
    Extra extra;
    Reports fast_reports;
    internal::HashMap<monitoring::ComparableTags, Reports,
                      monitoring::detail::UnorderedTagHash>
        tagged_reports;
  };

  auto essentials = std::make_unique<Essentials>();
  essentials->extra = std::move(extra);
  essentials->fast_reports = std::move(state->fast_reports);
  // To keep memory footprint manageble even when the user keeps creating new
  // tags, we clear internal storage of `tagged_reports` here.
  //
  // This does incur a recurring performance penalty when the same tag is
  // re-inserted, though.
  essentials->tagged_reports.swap(state->tagged_reports);

  state->fast_reports = Reports();  // Reset the counter.
  return essentials;
}

}  // namespace

void Report(
    Reading reading, const std::string_view& key,
    std::uint64_t value,  // Do we support `double`?
    std::initializer_list<std::pair<std::string_view, std::string_view>> tags) {
  InitializeMonitorTimerOnce();  // Start background timer if we haven't yet.

  if (FLARE_UNLIKELY(
          !pending_events->Get()->Emplace(reading, key, value, tags))) {
    FLARE_LOG_WARNING_EVERY_SECOND(
        "Report(s) to monitoring system are dropped due to internal buffer "
        "full.");
  }
}

}  // namespace monitoring

// `MonitoredCounter`.

MonitoredCounter::MonitoredCounter(
    std::string key,
    std::vector<std::pair<std::string, std::string>> extra_tags)
    : key_(key), extra_tags_(std::move(extra_tags)) {
  out_of_duty_registration_ =
      SetThreadOutOfDutyCallback([this] { FlushBufferedReports(); }, 100ms);
}

MonitoredCounter::~MonitoredCounter() {
  DeleteThreadOutOfDutyCallback(out_of_duty_registration_);
}

void MonitoredCounter::Add(
    std::uint64_t value,
    std::initializer_list<std::pair<std::string_view, std::string_view>> tags) {
  FLARE_CHECK_GE(value, 0);
  auto&& state = state_.Get();
  state->dirty = true;

  auto&& reports = state->tagged_reports.TryGet(tags);
  if (FLARE_UNLIKELY(!reports)) {
    reports = &state->tagged_reports[monitoring::AsComparableTags(tags)];
  }
  reports->sum += value;
  ++reports->times;
  FlushBufferCheck(state);
}

void MonitoredCounter::Increment(
    std::initializer_list<std::pair<std::string_view, std::string_view>> tags) {
  Add(1, tags);
}

void MonitoredCounter::FlushBufferedReports() noexcept {
  static constexpr auto kFlushInterval = 1s;
  state_->next_report = ReadCoarseSteadyClock() + kFlushInterval;
  if (!std::exchange(state_->dirty, false)) {
    return;
  }

  // Merge per-instace extra-tags with the global ones.
  auto extra_tags =
      monitoring::MergeTags(monitoring::GetGlobalExtraTags(), extra_tags_);

  // Save the states ASAP and leave the rest (constructing an instance of
  // `CoalescedCounterEvent`) to DPC.
  auto essentials = monitoring::SaveReportEssentialsAndClear<MonitoredCounter>(
      key_, std::pair(key_, extra_tags), state_.Get());

  // This DPC takes care of the rest.
  internal::QueueDpc([essentials = std::move(essentials)] {
    monitoring::CoalescedCounterEvent event;

    // The untagged reports.
    event.key = essentials->extra.first;
    event.tags = essentials->extra.second;
    event.sum = essentials->fast_reports.sum;
    event.times = essentials->fast_reports.times;
    monitoring::Dispatcher::Instance()->ReportCoalescedEvent(event);

    // The tagged ones.
    for (auto&& [tags, v] : essentials->tagged_reports) {
      event.tags =
          monitoring::MergeTags(essentials->extra.second, tags.GetTags());
      event.sum = v.sum;
      event.times = v.times;
      monitoring::Dispatcher::Instance()->ReportCoalescedEvent(event);
    }
  });
}

// `MonitoredGauge`.

MonitoredGauge::MonitoredGauge(
    std::string key,
    std::vector<std::pair<std::string, std::string>> extra_tags)
    : key_(key), extra_tags_(std::move(extra_tags)) {
  out_of_duty_registration_ =
      SetThreadOutOfDutyCallback([this] { FlushBufferedReports(); }, 100ms);
}

MonitoredGauge::~MonitoredGauge() {
  DeleteThreadOutOfDutyCallback(out_of_duty_registration_);
}

void MonitoredGauge::Add(
    std::int64_t value,
    std::initializer_list<std::pair<std::string_view, std::string_view>> tags) {
  FLARE_CHECK_GE(value, 0);
  Report(value, tags);
}

void MonitoredGauge::Subtract(
    std::int64_t value,
    std::initializer_list<std::pair<std::string_view, std::string_view>> tags) {
  FLARE_CHECK_GE(value, 0);
  Report(-value, tags);
}

void MonitoredGauge::Increment(
    std::initializer_list<std::pair<std::string_view, std::string_view>> tags) {
  Add(1, tags);
}

void MonitoredGauge::Decrement(
    std::initializer_list<std::pair<std::string_view, std::string_view>> tags) {
  Subtract(1, tags);
}

void MonitoredGauge::Report(
    std::int64_t value,
    std::initializer_list<std::pair<std::string_view, std::string_view>>
        tags) noexcept {
  auto&& state = state_.Get();
  state->dirty = true;

  auto&& reports = state->tagged_reports.TryGet(tags);
  if (FLARE_UNLIKELY(!reports)) {
    reports = &state->tagged_reports[monitoring::AsComparableTags(tags)];
  }
  reports->sum += value;
  ++reports->times;
  FlushBufferCheck(state);
}

// Embarrassingly the same as `MonitoredCounter`.
void MonitoredGauge::FlushBufferedReports() noexcept {
  static constexpr auto kFlushInterval = 1s;
  state_->next_report = ReadCoarseSteadyClock() + kFlushInterval;
  if (!std::exchange(state_->dirty, false)) {
    return;
  }

  auto extra_tags =
      monitoring::MergeTags(monitoring::GetGlobalExtraTags(), extra_tags_);
  auto essentials = monitoring::SaveReportEssentialsAndClear<MonitoredGauge>(
      key_, std::pair(key_, extra_tags), state_.Get());

  internal::QueueDpc([essentials = std::move(essentials)] {
    monitoring::CoalescedGaugeEvent event;

    event.key = essentials->extra.first;
    event.tags = essentials->extra.second;
    event.sum = essentials->fast_reports.sum;
    event.times = essentials->fast_reports.times;
    monitoring::Dispatcher::Instance()->ReportCoalescedEvent(event);

    for (auto&& [tags, v] : essentials->tagged_reports) {
      event.tags =
          monitoring::MergeTags(essentials->extra.second, tags.GetTags());
      event.sum = v.sum;
      event.times = v.times;
      monitoring::Dispatcher::Instance()->ReportCoalescedEvent(event);
    }
  });
}

// `MonitoredTimer`.

MonitoredTimer::MonitoredTimer(
    std::string key,
    std::vector<std::pair<std::string, std::string>> extra_tags)
    : MonitoredTimer(std::move(key), std::chrono::microseconds(1),
                     std::move(extra_tags)) {}

MonitoredTimer::MonitoredTimer(
    std::string key, std::chrono::nanoseconds unit,
    std::vector<std::pair<std::string, std::string>> extra_tags)
    : key_(key), unit_(unit), extra_tags_(std::move(extra_tags)) {
  // We possible, we do a divide-by-constant here. The compiler is likely able
  // to generate code in a more optimized way than a naive 64-bit idiv.
  if (unit == 1ns) {
    as_count_ = [](auto, auto d) -> std::uint64_t { return d / 1ns; };
  } else if (unit == 1us) {
    as_count_ = [](auto, auto d) -> std::uint64_t { return d / 1us; };
  } else if (unit == 1ms) {
    as_count_ = [](auto, auto d) -> std::uint64_t { return d / 1ms; };
  } else if (unit == 1s) {
    as_count_ = [](auto, auto d) -> std::uint64_t { return d / 1s; };
  } else {
    // Really bad luck then.
    as_count_ = [](auto me, auto d) -> std::uint64_t { return d / me->unit_; };
  }
  out_of_duty_registration_ =
      SetThreadOutOfDutyCallback([this] { FlushBufferedReports(); }, 100ms);
}

MonitoredTimer::~MonitoredTimer() {
  DeleteThreadOutOfDutyCallback(out_of_duty_registration_);
}

void MonitoredTimer::Report(
    std::chrono::nanoseconds duration,
    std::initializer_list<std::pair<std::string_view, std::string_view>> tags) {
  auto&& state = state_.Get();
  state->dirty = true;

  auto&& reports = state->tagged_reports.TryGet(tags);
  if (FLARE_UNLIKELY(!reports)) {
    reports = &state->tagged_reports[monitoring::AsComparableTags(tags)];
  }
  if (auto count = as_count_(this, duration);
      count < kOptimizedForDurationThreshold) {
    ++reports->fast_times[count];
  } else {
    ++reports->times[count];
  }
  FlushBufferCheck(state);
}

void MonitoredTimer::FlushBufferedReports() noexcept {
  static constexpr auto kFlushInterval = 1s;
  state_->next_report = ReadCoarseSteadyClock() + kFlushInterval;
  if (!std::exchange(state_->dirty, false)) {
    return;
  }

  auto extra_tags =
      monitoring::MergeTags(monitoring::GetGlobalExtraTags(), extra_tags_);
  auto essentials = monitoring::SaveReportEssentialsAndClear<MonitoredTimer>(
      key_, std::tuple(key_, unit_, extra_tags), state_.Get());

  internal::QueueDpc([essentials = std::move(essentials)] {
    auto&& [key, unit, extra_tags] = essentials->extra;
    monitoring::CoalescedTimerEvent event;

    event.key = key;
    event.unit = unit;
    event.tags = extra_tags;

    // Being slow does not matter much, we're already in DPC here.
    auto read_times = [](auto&& report, auto unit) {
      std::vector<std::pair<std::chrono::nanoseconds, std::size_t>> result;
      for (auto&& [k, v] : report.times) {
        FLARE_CHECK_GE(k, kOptimizedForDurationThreshold);
        result.push_back(std::make_pair(k * unit, v));
      }
      // Don't forget to merge fast timers in.
      for (int i = 0; i != kOptimizedForDurationThreshold; ++i) {
        if (auto c = report.fast_times[i]) {
          result.push_back(std::make_pair(i * unit, c));
        }
      }
      return result;
    };

    event.times = read_times(essentials->fast_reports, unit);
    monitoring::Dispatcher::Instance()->ReportCoalescedEvent(event);

    for (auto&& [tags, v] : essentials->tagged_reports) {
      event.tags = monitoring::MergeTags(extra_tags, tags.GetTags());
      event.times = read_times(v, unit);
      monitoring::Dispatcher::Instance()->ReportCoalescedEvent(event);
    }
  });
}

}  // namespace flare
