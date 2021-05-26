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

#ifndef FLARE_BASE_WRITE_MOSTLY_METRICS_H_
#define FLARE_BASE_WRITE_MOSTLY_METRICS_H_

#include <algorithm>
#include <limits>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "thirdparty/googletest/gtest/gtest_prod.h"

#include "flare/base/align.h"
#include "flare/base/chrono.h"
#include "flare/base/internal/time_keeper.h"
#include "flare/base/thread/spinlock.h"
#include "flare/base/write_mostly/write_mostly.h"

namespace flare {

namespace write_mostly::detail {

template <class T>
struct MetricsStats;

template <class T>
struct MetricsTraits;

template <class T>
using MetricsWriteMostly = WriteMostly<MetricsTraits<T>>;

}  // namespace write_mostly::detail

template <class T>
class WriteMostlyMetrics {
 public:
  // in seconds, maintain the records of each second in the past hour.
  static constexpr int kMaxWindowSize = 3600;
  struct Result {
    T min, max, average;
    std::size_t cnt;
    bool operator==(const Result& other) const {
      return min == other.min && max == other.max && average == other.average &&
             cnt == other.cnt;
    }
  };

  WriteMostlyMetrics()
      : metrics_stats_records_(kMaxWindowSize), current_pos_(0) {
    timer_id_ = internal::TimeKeeper::Instance()->AddTimer(
        ReadSteadyClock() + std::chrono::seconds(1), std::chrono::seconds(1),
        [this](uint64_t) { Purge(); }, false);
  }

  ~WriteMostlyMetrics() {
    // FIXME: We need to wait for time to be fully stopped.
    internal::TimeKeeper::Instance()->KillTimer(timer_id_);
  }

  void Report(const T& value) {
    metrics_.Update(write_mostly::detail::MetricsStats(value));
  }

  // Get metrics result of recent n seconds.
  // Maximum is 3600(1 hour).
  Result Get(std::size_t seconds) const {
    std::scoped_lock _(records_lock_);
    if (seconds > kMaxWindowSize) {
      seconds = kMaxWindowSize;
    }
    std::size_t pos = current_pos_;
    write_mostly::detail::MetricsStats<T> stats;
    for (std::size_t i = 0; i < seconds; ++i) {
      pos = (pos + kMaxWindowSize - 1) % kMaxWindowSize;
      if (!metrics_stats_records_[pos]) {
        break;
      }
      stats.Merge(*metrics_stats_records_[pos]);
    }
    return GetMetricsResult(stats);
  }

  // Get metrics result of recent second/minute/hour and total.
  Result GetAll() const {
    std::scoped_lock _(records_lock_);
    return GetMetricsResult(total_);
  }

 private:
  FRIEND_TEST(MetricsTest, Get);
  void Purge() {
    auto entry = std::make_unique<write_mostly::detail::MetricsStats<T>>();
    std::scoped_lock _(records_lock_);
    *entry = metrics_.Purge();
    total_.Merge(*entry);
    metrics_stats_records_[current_pos_] = std::move(entry);
    current_pos_ = (current_pos_ + 1) % kMaxWindowSize;
  }

  Result GetMetricsResult(
      const write_mostly::detail::MetricsStats<T>& stat) const {
    return Result{
        (stat.cnt > 0) ? stat.min : T(), (stat.cnt > 0) ? stat.max : T(),
        static_cast<T>((stat.cnt > 0) ? (stat.sum / stat.cnt) : T()), stat.cnt};
  }

  mutable std::mutex records_lock_;
  std::vector<std::unique_ptr<write_mostly::detail::MetricsStats<T>>>
      metrics_stats_records_;
  std::size_t current_pos_;
  write_mostly::detail::MetricsWriteMostly<T> metrics_;
  write_mostly::detail::MetricsStats<T> total_;
  uint64_t timer_id_;
};

namespace write_mostly::detail {

template <class T>
struct MetricsStats {
  constexpr MetricsStats()
      : sum(T()),
        cnt(0),
        max{std::numeric_limits<T>::min()},
        min(std::numeric_limits<T>::max()) {}
  explicit MetricsStats(const T& val) : sum(val), cnt(1), max(val), min(val) {}
  MetricsStats(const T& s, std::size_t c, const T& ma, const T& mi)
      : sum(s), cnt(c), max(ma), min(mi) {}
  void Merge(const MetricsStats<T>& other) {
    sum += other.sum;
    cnt += other.cnt;
    if (FLARE_UNLIKELY(other.max > max)) {
      max = other.max;
    }
    if (FLARE_UNLIKELY(other.min < min)) {
      min = other.min;
    }
  }

  using SumType = std::conditional_t<
      std::is_integral_v<T>,
      std::conditional_t<std::is_signed_v<T>, std::int64_t, std::uint64_t>,
      long double>;

  SumType sum;
  std::size_t cnt;
  T max;
  T min;
};

template <class T>
struct AtomicMetricsStats {
  constexpr AtomicMetricsStats() {}
  explicit AtomicMetricsStats(const MetricsStats<T>& stats)
      : metrics_stats(stats) {}

  void Update(const MetricsStats<T>& other) {
    std::scoped_lock lk(splk);
    metrics_stats.Merge(other);
  }

  void Merge(const AtomicMetricsStats& other) {
    std::scoped_lock _(splk, other.splk);
    metrics_stats.Merge(other.metrics_stats);
  }

  void CopyFrom(const AtomicMetricsStats<T>& src) {
    std::scoped_lock _(splk, src.splk);
    metrics_stats = src.metrics_stats;
  }

  MetricsStats<T> Read() const {
    std::scoped_lock _(splk);
    return metrics_stats;
  }

  AtomicMetricsStats<T> Purge() {
    MetricsStats<T> result;
    std::scoped_lock lk(splk);
    std::swap(metrics_stats, result);
    return AtomicMetricsStats<T>(result);
  }

  mutable Spinlock splk;
  MetricsStats<T> metrics_stats;
};

template <class T>
struct MetricsTraits {
  using Type = MetricsStats<T>;
  using WriteBuffer = AtomicMetricsStats<T>;
  static constexpr auto kWriteBufferInitializer = WriteBuffer();
  static void Update(WriteBuffer* wb, const MetricsStats<T>& val) {
    wb->Update(val);
  }
  static void Merge(WriteBuffer* wb1, const WriteBuffer& wb2) {
    wb1->Merge(wb2);
  }
  static void Copy(const WriteBuffer& src_wb, WriteBuffer* dst_wb) {
    dst_wb->CopyFrom(src_wb);
  }
  static MetricsStats<T> Read(const WriteBuffer& wb) { return wb.Read(); }
  static WriteBuffer Purge(WriteBuffer* wb) { return wb->Purge(); }
};

}  // namespace write_mostly::detail

}  // namespace flare

#endif  // FLARE_BASE_WRITE_MOSTLY_METRICS_H_
