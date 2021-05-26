// Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef FLARE_BASE_LOGGING_H_
#define FLARE_BASE_LOGGING_H_

#include "flare/base/chrono.h"
#include "flare/base/internal/logging.h"
#include "flare/base/likely.h"

// Macros here are more performant than their counterparts in glog. Consider
// use macro here unless you have a reason not to do so.

// These macros are defined separately (in `internal/logging.h`, if you're
// curious), they're shown here for exposition only.
//
// You should use the same argument for `std::format` for `...` part in the
// declaration (@sa: `doc/logging.md`.), e.g.:
//
// FLARE_LOG_INFO("My fancy string is [{}].", str1);

// #define FLARE_VLOG(n, ...)
//
// #define FLARE_CHECK(expr, ...)
// #define FLARE_CHECK_EQ(val1, val2, ...)
// #define FLARE_CHECK_NE(val1, val2, ...)
// #define FLARE_CHECK_LE(val1, val2, ...)
// #define FLARE_CHECK_LT(val1, val2, ...)
// #define FLARE_CHECK_GE(val1, val2, ...)
// #define FLARE_CHECK_GT(val1, val2, ...)
// #define FLARE_CHECK_NEAR(val1, val2, margin, ...)
//
// #define FLARE_LOG_INFO(...)
// #define FLARE_LOG_WARNING(...)
// #define FLARE_LOG_ERROR(...)
// #define FLARE_LOG_FATAL(...)
//
// #define FLARE_LOG_INFO_IF(expr, ...)
// #define FLARE_LOG_WARNING_IF(expr, ...)
// #define FLARE_LOG_ERROR_IF(expr, ...)
// #define FLARE_LOG_FATAL_IF(expr, ...)
//
// #define FLARE_LOG_INFO_EVERY_N(N, ...)
// #define FLARE_LOG_WARNING_EVERY_N(N, ...)
// #define FLARE_LOG_ERROR_EVERY_N(N, ...)
// #define FLARE_LOG_FATAL_EVERY_N(N, ...)
//
// #define FLARE_LOG_INFO_IF_EVERY_N(expr, N, ...)
// #define FLARE_LOG_WARNING_IF_EVERY_N(expr, N, ...)
// #define FLARE_LOG_ERROR_IF_EVERY_N(expr, N, ...)
// #define FLARE_LOG_FATAL_IF_EVERY_N(expr, N, ...)
//
// #define FLARE_LOG_INFO_ONCE(...)
// #define FLARE_LOG_WARNING_ONCE(...)
// #define FLARE_LOG_ERROR_ONCE(...)
// #define FLARE_LOG_FATAL_ONCE(...)
//
// #define FLARE_LOG_INFO_IF_ONCE(expr, ...)
// #define FLARE_LOG_WARNING_IF_ONCE(expr, ...)
// #define FLARE_LOG_ERROR_IF_ONCE(expr, ...)
// #define FLARE_LOG_FATAL_IF_ONCE(expr, ...)
//
// #define FLARE_DCHECK(expr, ...)
// #define FLARE_DCHECK_EQ(val1, val2, ...)
// #define FLARE_DCHECK_NE(val1, val2, ...)
// #define FLARE_DCHECK_LE(val1, val2, ...)
// #define FLARE_DCHECK_LT(val1, val2, ...)
// #define FLARE_DCHECK_GE(val1, val2, ...)
// #define FLARE_DCHECK_GT(val1, val2, ...)
// #define FLARE_DCHECK_NEAR(val1, val2, margin, ...)
//
// #define FLARE_DLOG_INFO(...)
// #define FLARE_DLOG_WARNING(...)
// #define FLARE_DLOG_ERROR(...)
// #define FLARE_DLOG_FATAL(...)
//
// #define FLARE_DLOG_INFO_IF(expr, ...)
// #define FLARE_DLOG_WARNING_IF(expr, ...)
// #define FLARE_DLOG_ERROR_IF(expr, ...)
// #define FLARE_DLOG_FATAL_IF(expr, ...)
//
// #define FLARE_DLOG_INFO_EVERY_N(N, ...)
// #define FLARE_DLOG_WARNING_EVERY_N(N, ...)
// #define FLARE_DLOG_ERROR_EVERY_N(N, ...)
// #define FLARE_DLOG_FATAL_EVERY_N(N, ...)
//
// #define FLARE_DLOG_INFO_ONCE(...)
// #define FLARE_DLOG_WARNING_ONCE(...)
// #define FLARE_DLOG_ERROR_ONCE(...)
// #define FLARE_DLOG_FATAL_ONCE(...)
//
// #define FLARE_PCHECK(expr, ...)
//
// #define FLARE_PLOG_INFO(...)
// #define FLARE_PLOG_WARNING(...)
// #define FLARE_PLOG_ERROR(...)
// #define FLARE_PLOG_FATAL(...)
//
// #define FLARE_PLOG_INFO_IF(expr, ...)
// #define FLARE_PLOG_WARNING_IF(expr, ...)
// #define FLARE_PLOG_ERROR_IF(expr, ...)
// #define FLARE_PLOG_FATAL_IF(expr, ...)

// Inspired by brpc, credits to them.
//
// https://github.com/apache/incubator-brpc/blob/master/docs/cn/streaming_log.md
#define FLARE_LOG_INFO_EVERY_SECOND(...) \
  FLARE_DETAIL_LOGGING_LOG_EVERY_N_SECOND_IMPL(1, INFO, __VA_ARGS__)
#define FLARE_LOG_WARNING_EVERY_SECOND(...) \
  FLARE_DETAIL_LOGGING_LOG_EVERY_N_SECOND_IMPL(1, WARNING, __VA_ARGS__)
#define FLARE_LOG_ERROR_EVERY_SECOND(...) \
  FLARE_DETAIL_LOGGING_LOG_EVERY_N_SECOND_IMPL(1, ERROR, __VA_ARGS__)
#define FLARE_LOG_FATAL_EVERY_SECOND(...) \
  FLARE_DETAIL_LOGGING_LOG_EVERY_N_SECOND_IMPL(1, FATAL, __VA_ARGS__)

#define FLARE_LOG_INFO_IF_EVERY_SECOND(expr, ...) \
  FLARE_DETAIL_LOGGING_LOG_EVERY_N_SECOND_IF_IMPL(expr, 1, INFO, __VA_ARGS__)
#define FLARE_LOG_WARNING_IF_EVERY_SECOND(expr, ...)                       \
  FLARE_DETAIL_LOGGING_LOG_EVERY_N_SECOND_IF_IMPL(FLARE_UNLIKELY(expr), 1, \
                                                  WARNING, __VA_ARGS__)
#define FLARE_LOG_ERROR_IF_EVERY_SECOND(expr, ...)                         \
  FLARE_DETAIL_LOGGING_LOG_EVERY_N_SECOND_IF_IMPL(FLARE_UNLIKELY(expr), 1, \
                                                  ERROR, __VA_ARGS__)
#define FLARE_LOG_FATAL_IF_EVERY_SECOND(expr, ...)                         \
  FLARE_DETAIL_LOGGING_LOG_EVERY_N_SECOND_IF_IMPL(FLARE_UNLIKELY(expr), 1, \
                                                  FATAL, __VA_ARGS__)

///////////////////////////////////////
// Implementation goes below.        //
///////////////////////////////////////

namespace flare {

namespace detail::logging {

inline struct LoggingLinkCheck { LoggingLinkCheck(); } link_check;

}  // namespace detail::logging
}  // namespace flare

// Clang 10 has not implemented P1381R1 yet.
#if defined(__clang__) && __clang__ <= 10

#define FLARE_DETAIL_LOGGING_LOG_EVERY_N_SECOND_IMPL(N, severity, ...)        \
  do {                                                                        \
    static ::std::atomic<::std::chrono::nanoseconds>                          \
        flare_anonymous_log_last_occurs{};                                    \
    if (auto flare_anonymous_now =                                            \
            ::flare::ReadCoarseSteadyClock().time_since_epoch();              \
        FLARE_UNLIKELY(flare_anonymous_log_last_occurs.load(                  \
                           ::std::memory_order_relaxed) +                     \
                           ::std::chrono::seconds(N) <                        \
                       flare_anonymous_now)) {                                \
      static ::std::atomic<bool> flare_anonymous_logging{false};              \
      if (!flare_anonymous_logging.exchange(true,                             \
                                            ::std::memory_order_seq_cst)) {   \
        auto flare_anonymous_really_write =                                   \
            flare_anonymous_log_last_occurs.load(                             \
                ::std::memory_order_relaxed) +                                \
                ::std::chrono::seconds(N) <                                   \
            flare_anonymous_now;                                              \
        if (flare_anonymous_really_write) {                                   \
          flare_anonymous_log_last_occurs.store(flare_anonymous_now,          \
                                                ::std::memory_order_relaxed); \
        }                                                                     \
        flare_anonymous_logging.store(false, ::std::memory_order_seq_cst);    \
        if (flare_anonymous_really_write) {                                   \
          ::google::LogMessage(__FILE__, __LINE__, ::google::GLOG_##severity, \
                               0, &::google::LogMessage::SendToLog)           \
                  .stream()                                                   \
              << ::flare::internal::logging::FormatLog(__FILE__, __LINE__,    \
                                                       __VA_ARGS__);          \
        }                                                                     \
      }                                                                       \
    }                                                                         \
  } while (0)

#else

#define FLARE_DETAIL_LOGGING_LOG_EVERY_N_SECOND_IMPL(N, severity, ...)         \
  do {                                                                         \
    /* Read / write (but not RMW) comes with no cost in `std::atomic<...>`, */ \
    /* so we make it safe. */                                                  \
    static ::std::atomic<::std::chrono::nanoseconds>                           \
        flare_anonymous_log_last_occurs{};                                     \
    /* You're not expected to see the log too often if you use this macro, */  \
    /* aren't you? */                                                          \
    if (auto flare_anonymous_now =                                             \
            ::flare::ReadCoarseSteadyClock().time_since_epoch();               \
        FLARE_UNLIKELY(flare_anonymous_log_last_occurs.load(                   \
                           ::std::memory_order_relaxed) +                      \
                           ::std::chrono::seconds(N) <                         \
                       flare_anonymous_now)) {                                 \
      /* Executed every second. I think it's "cold" enough to be moved out. */ \
      [&]() FLARE_INTERNAL_DETAIL_LOGGING_ATTRIBUTE_NOINLINE_COLD {            \
        static ::std::atomic<bool> flare_anonymous_logging{false};             \
        /* To ensure only a single thread really writes. */                    \
        /* Full barrier here to enforce order between atomics. */              \
        if (!flare_anonymous_logging.exchange(true,                            \
                                              ::std::memory_order_seq_cst)) {  \
          /* DCLP. */                                                          \
          auto flare_anonymous_really_write =                                  \
              flare_anonymous_log_last_occurs.load(                            \
                  ::std::memory_order_relaxed) +                               \
                  ::std::chrono::seconds(N) <                                  \
              flare_anonymous_now;                                             \
          if (flare_anonymous_really_write) {                                  \
            flare_anonymous_log_last_occurs.store(                             \
                flare_anonymous_now, ::std::memory_order_relaxed);             \
          }                                                                    \
          /* Now that we've determined whether we should write and updated */  \
          /* the timestamp, free the lock ASAP. */                             \
          flare_anonymous_logging.store(false, ::std::memory_order_seq_cst);   \
          if (flare_anonymous_really_write) {                                  \
            /* The log may be written now. */                                  \
            ::google::LogMessage(                                              \
                __FILE__, __LINE__, ::google::GLOG_##severity,                 \
                0, /* Number of occurrance. Not supplied for perf. reasons. */ \
                &::google::LogMessage::SendToLog)                              \
                    .stream()                                                  \
                << ::flare::internal::logging::FormatLog(__FILE__, __LINE__,   \
                                                         __VA_ARGS__);         \
          }                                                                    \
        }                                                                      \
      }();                                                                     \
    }                                                                          \
  } while (0)

#endif

#define FLARE_DETAIL_LOGGING_LOG_EVERY_N_SECOND_IF_IMPL(expr, N, severity,    \
                                                        ...)                  \
  do {                                                                        \
    if (expr) {                                                               \
      FLARE_DETAIL_LOGGING_LOG_EVERY_N_SECOND_IMPL(N, severity, __VA_ARGS__); \
    }                                                                         \
  } while (0)

#endif  // FLARE_BASE_LOGGING_H_
