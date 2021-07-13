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

#ifndef FLARE_BASE_INTERNAL_TIME_VIEW_H_
#define FLARE_BASE_INTERNAL_TIME_VIEW_H_

#include "flare/base/chrono.h"
#include "flare/base/internal/meta.h"

namespace flare::internal {

namespace detail::time_view {

// Read time from `Clock`. This method use our own `ReadXxxClock` whenever
// possible.
template <class Clock>
auto ReadClock() {
  if constexpr (std::is_same_v<Clock, std::chrono::system_clock>) {
    return ReadSystemClock();
  } else if constexpr (std::is_same_v<Clock, std::chrono::steady_clock>) {
    return ReadSteadyClock();
  } else {
    return Clock::now();
  }
}

// Cast time point to `TimePoint`.
template <class TimePoint, class Clock, class Duration,
          class = typename TimePoint::clock>
auto CastTo(std::chrono::time_point<Clock, Duration> time) {
  if constexpr (std::is_same_v<typename TimePoint::clock, Clock>) {
    return time;  // Nothing to convert.
  } else {
    return ReadClock<typename TimePoint::clock>() + (time - ReadClock<Clock>());
  }
}

// Cast duration to `TimePoint`.
template <class TimePoint, class Rep, class Period,
          class = typename TimePoint::clock>
auto CastTo(std::chrono::duration<Rep, Period> duration) {
  return ReadClock<typename TimePoint::clock>() + duration;
}

}  // namespace detail::time_view

// This helper class receives either time duration or time point, and convert
// whatever it receives to `Expecting`.
template <class Expecting, class = void>
class TimeView {
  static_assert(
      sizeof(Expecting) == 0,
      "`Expecting` should be specified as either a time point or a duration.");
};

// Handles the case when `Expecting` (@sa: main template) is actually a time
// point.
template <class TimePoint>
class TimeView<TimePoint, void_t<typename TimePoint::clock>> {
 public:
  constexpr TimeView() = default;

  // Accepts a `std::chrono::duration` and convert it to `TimePoint`.
  template <class Rep, class Period>
  constexpr /* implicit */ TimeView(std::chrono::duration<Rep, Period> duration)
      : time_point_(detail::time_view::CastTo<TimePoint>(duration)) {}

  // Accepts a time point in `Clock`, and convert it to `TimePoint`.
  template <class Clock, class Duration>
  constexpr /* implicit */ TimeView(
      std::chrono::time_point<Clock, Duration> time)
      : time_point_(detail::time_view::CastTo<TimePoint>(time)) {}

  // Read the time point we've received.
  constexpr const TimePoint& Get() const noexcept { return time_point_; }

 private:
  TimePoint time_point_{};
};

// Handles the case when `Expecting` (@sa: main template) is actually a
// duration.
template <class Duration>
class TimeView<Duration,
               // Quick & dirty trick to test if `Duration` is indeed a
               // duration.
               void_t<decltype(std::declval<Duration>().count())>> {
 public:
  constexpr TimeView() = default;

  // Accepts a `std::chrono::duration` and convert it to `Duration`.
  template <class Rep, class Period>
  constexpr /* implicit */ TimeView(std::chrono::duration<Rep, Period> duration)
      : duration_(duration) {}

  // Accepts a time point in `Clock`, and convert it to `Duration`.
  template <class Clock, class AcceptingDuration>
  constexpr /* implicit */ TimeView(
      std::chrono::time_point<Clock, AcceptingDuration> time)
      : duration_(time - detail::time_view::ReadClock<Clock>()) {}

  // Read the time point we've received.
  constexpr const Duration& Get() const noexcept { return duration_; }

 private:
  Duration duration_{};
};

// We use `std::chrono::steady_clock::time_point` extensively in our code, it
// warrants its own alias.
using SteadyClockView = TimeView<std::chrono::steady_clock::time_point>;
// `std::chrono::system_clock::time_point` is widely used for interacting with
// humans, it warrants its own alias too.
using SystemClockView = TimeView<std::chrono::system_clock::time_point>;

// `std::chrono::nanoseconds` also warrants its alias.
using NanosecondsView = TimeView<std::chrono::nanoseconds>;

}  // namespace flare::internal

#endif  // FLARE_BASE_INTERNAL_TIME_VIEW_H_
