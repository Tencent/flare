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

#ifndef FLARE_BASE_INTERNAL_LOGGING_H_
#define FLARE_BASE_INTERNAL_LOGGING_H_

// DO NOT USE THIS HEADER. USE `flare/base/logging.h` INSTEAD.

// To avoid circular dependency chain, we implement "basic" logging facility
// here, for other "low-level" components to use.

#include <atomic>
#include <string>

#include "fmt/format.h"
#include "fmt/ostream.h"
#include "glog/logging.h"

#include "flare/base/internal/macro.h"
#include "flare/base/likely.h"

// We might want to insert our prefix (passed via thread-local / fiber-local
// string) in logging for debugging purpose in the future.

// glog's CHECK_OP does not optimize well by GCC 8.2. `google::CheckOpString`
// tries to hint the compiler, but the compiler does not seem to recognize that.
// In the meantime, `CHECK_OP` produces really large amount of code which can
// hurt performance. Our own implementation avoid these limitations.
#define FLARE_CHECK(expr, ...) \
  FLARE_INTERNAL_DETAIL_LOGGING_CHECK(expr, ##__VA_ARGS__)
#define FLARE_CHECK_EQ(val1, val2, ...)   \
  FLARE_INTERNAL_DETAIL_LOGGING_CHECK_OP( \
      _EQ, ==, val1, val2, ##__VA_ARGS__)  // `##` is GNU extension.
#define FLARE_CHECK_NE(val1, val2, ...) \
  FLARE_INTERNAL_DETAIL_LOGGING_CHECK_OP(_NE, !=, val1, val2, ##__VA_ARGS__)
#define FLARE_CHECK_LE(val1, val2, ...) \
  FLARE_INTERNAL_DETAIL_LOGGING_CHECK_OP(_LE, <=, val1, val2, ##__VA_ARGS__)
#define FLARE_CHECK_LT(val1, val2, ...) \
  FLARE_INTERNAL_DETAIL_LOGGING_CHECK_OP(_LT, <, val1, val2, ##__VA_ARGS__)
#define FLARE_CHECK_GE(val1, val2, ...) \
  FLARE_INTERNAL_DETAIL_LOGGING_CHECK_OP(_GE, >=, val1, val2, ##__VA_ARGS__)
#define FLARE_CHECK_GT(val1, val2, ...) \
  FLARE_INTERNAL_DETAIL_LOGGING_CHECK_OP(_GT, >, val1, val2, ##__VA_ARGS__)
#define FLARE_CHECK_NEAR(val1, val2, margin, ...)             \
  do {                                                        \
    FLARE_CHECK_LE((val1), (val2) + (margin), ##__VA_ARGS__); \
    FLARE_CHECK_GE((val1), (val2) - (margin), ##__VA_ARGS__); \
  } while (0)

// Do NOT use `DCHECK`s from glog, they're not `constexpr`-friendly.
#ifndef NDEBUG
#define FLARE_DCHECK(expr, ...) FLARE_CHECK(expr, ##__VA_ARGS__)
#define FLARE_DCHECK_EQ(expr1, expr2, ...) \
  FLARE_CHECK_EQ(expr1, expr2, ##__VA_ARGS__)
#define FLARE_DCHECK_NE(expr1, expr2, ...) \
  FLARE_CHECK_NE(expr1, expr2, ##__VA_ARGS__)
#define FLARE_DCHECK_LE(expr1, expr2, ...) \
  FLARE_CHECK_LE(expr1, expr2, ##__VA_ARGS__)
#define FLARE_DCHECK_GE(expr1, expr2, ...) \
  FLARE_CHECK_GE(expr1, expr2, ##__VA_ARGS__)
#define FLARE_DCHECK_LT(expr1, expr2, ...) \
  FLARE_CHECK_LT(expr1, expr2, ##__VA_ARGS__)
#define FLARE_DCHECK_GT(expr1, expr2, ...) \
  FLARE_CHECK_GT(expr1, expr2, ##__VA_ARGS__)
#define FLARE_DCHECK_NEAR(expr1, expr2, margin, ...) \
  FLARE_CHECK_NEAR(expr1, expr2, ##__VA_ARGS__)
#else
#define FLARE_DCHECK(expr, ...) \
  while (0) FLARE_CHECK(expr, ##__VA_ARGS__)
#define FLARE_DCHECK_EQ(expr1, expr2, ...) \
  while (0) FLARE_CHECK_EQ(expr1, expr2, ##__VA_ARGS__)
#define FLARE_DCHECK_NE(expr1, expr2, ...) \
  while (0) FLARE_CHECK_NE(expr1, expr2, ##__VA_ARGS__)
#define FLARE_DCHECK_LE(expr1, expr2, ...) \
  while (0) FLARE_CHECK_LE(expr1, expr2, ##__VA_ARGS__)
#define FLARE_DCHECK_GE(expr1, expr2, ...) \
  while (0) FLARE_CHECK_GE(expr1, expr2, ##__VA_ARGS__)
#define FLARE_DCHECK_LT(expr1, expr2, ...) \
  while (0) FLARE_CHECK_LT(expr1, expr2, ##__VA_ARGS__)
#define FLARE_DCHECK_GT(expr1, expr2, ...) \
  while (0) FLARE_CHECK_GT(expr1, expr2, ##__VA_ARGS__)
#define FLARE_DCHECK_NEAR(expr1, expr2, margin, ...) \
  while (0) FLARE_CHECK_NEAR(expr1, expr2, ##__VA_ARGS__)
#endif

#define FLARE_PCHECK(expr, ...) \
  FLARE_INTERNAL_DETAIL_LOGGING_PCHECK(expr, ##__VA_ARGS__)

#define FLARE_VLOG(n, ...)                                         \
  LOG_IF(INFO, FLARE_UNLIKELY(VLOG_IS_ON(n)))                      \
      << ::flare::internal::logging::FormatLog(__FILE__, __LINE__, \
                                               __VA_ARGS__)

#define FLARE_LOG_INFO(...)                                              \
  LOG(INFO) << ::flare::internal::logging::FormatLog(__FILE__, __LINE__, \
                                                     __VA_ARGS__)
#define FLARE_LOG_WARNING(...)                                              \
  LOG(WARNING) << ::flare::internal::logging::FormatLog(__FILE__, __LINE__, \
                                                        __VA_ARGS__)
#define FLARE_LOG_ERROR(...)                                              \
  LOG(ERROR) << ::flare::internal::logging::FormatLog(__FILE__, __LINE__, \
                                                      __VA_ARGS__)
#define FLARE_LOG_FATAL(...)                                                \
  do {                                                                      \
    LOG(FATAL) << ::flare::internal::logging::FormatLog(__FILE__, __LINE__, \
                                                        __VA_ARGS__);       \
  } while (0);                                                              \
  FLARE_UNREACHABLE()

#define FLARE_LOG_INFO_IF(expr, ...)                           \
  LOG_IF(INFO, expr) << ::flare::internal::logging::FormatLog( \
      __FILE__, __LINE__, __VA_ARGS__)
#define FLARE_LOG_WARNING_IF(expr, ...)                            \
  LOG_IF(WARNING, FLARE_UNLIKELY(expr))                            \
      << ::flare::internal::logging::FormatLog(__FILE__, __LINE__, \
                                               __VA_ARGS__)
#define FLARE_LOG_ERROR_IF(expr, ...)                              \
  LOG_IF(ERROR, FLARE_UNLIKELY(expr))                              \
      << ::flare::internal::logging::FormatLog(__FILE__, __LINE__, \
                                               __VA_ARGS__)
#define FLARE_LOG_FATAL_IF(expr, ...)                              \
  LOG_IF(FATAL, FLARE_UNLIKELY(expr))                              \
      << ::flare::internal::logging::FormatLog(__FILE__, __LINE__, \
                                               __VA_ARGS__)

#define FLARE_LOG_INFO_EVERY_N(N, ...) \
  FLARE_INTERNAL_DETAIL_LOG_EVERY_N(INFO, N, __VA_ARGS__)
#define FLARE_LOG_WARNING_EVERY_N(N, ...) \
  FLARE_INTERNAL_DETAIL_LOG_EVERY_N(WARNING, N, __VA_ARGS__)
#define FLARE_LOG_ERROR_EVERY_N(N, ...) \
  FLARE_INTERNAL_DETAIL_LOG_EVERY_N(ERROR, N, __VA_ARGS__)
#define FLARE_LOG_FATAL_EVERY_N(N, ...) \
  FLARE_INTERNAL_DETAIL_LOG_EVERY_N(FATAL, N, __VA_ARGS__)

#define FLARE_LOG_INFO_IF_EVERY_N(expr, N, ...) \
  FLARE_INTERNAL_DETAIL_LOG_IF_EVERY_N(expr, INFO, N, __VA_ARGS__)
#define FLARE_LOG_WARNING_IF_EVERY_N(expr, N, ...) \
  FLARE_INTERNAL_DETAIL_LOG_IF_EVERY_N(expr, WARNING, N, __VA_ARGS__)
#define FLARE_LOG_ERROR_IF_EVERY_N(expr, N, ...) \
  FLARE_INTERNAL_DETAIL_LOG_IF_EVERY_N(expr, ERROR, N, __VA_ARGS__)
#define FLARE_LOG_FATAL_IF_EVERY_N(expr, N, ...) \
  FLARE_INTERNAL_DETAIL_LOG_IF_EVERY_N(expr, FATAL, N, __VA_ARGS__)

#define FLARE_LOG_INFO_ONCE(...) \
  FLARE_INTERNAL_DETAIL_LOG_ONCE(INFO, __VA_ARGS__)
#define FLARE_LOG_WARNING_ONCE(...) \
  FLARE_INTERNAL_DETAIL_LOG_ONCE(WARNING, __VA_ARGS__)
#define FLARE_LOG_ERROR_ONCE(...) \
  FLARE_INTERNAL_DETAIL_LOG_ONCE(ERROR, __VA_ARGS__)
#define FLARE_LOG_FATAL_ONCE(...)                        \
  /* You're unlikely to have a second chance anyway.. */ \
  FLARE_INTERNAL_DETAIL_LOG_ONCE(FATAL, __VA_ARGS__)

#define FLARE_LOG_INFO_IF_ONCE(expr, ...)                \
  do {                                                   \
    if (expr) {                                          \
      FLARE_INTERNAL_DETAIL_LOG_ONCE(INFO, __VA_ARGS__); \
    }                                                    \
  } while (0)
#define FLARE_LOG_WARNING_IF_ONCE(expr, ...)                \
  do {                                                      \
    if (FLARE_UNLIKELY(expr)) {                             \
      FLARE_INTERNAL_DETAIL_LOG_ONCE(WARNING, __VA_ARGS__); \
    }                                                       \
  } while (0)
#define FLARE_LOG_ERROR_IF_ONCE(expr, ...)                \
  do {                                                    \
    if (FLARE_UNLIKELY(expr)) {                           \
      FLARE_INTERNAL_DETAIL_LOG_ONCE(ERROR, __VA_ARGS__); \
    }                                                     \
  } while (0)
#define FLARE_LOG_FATAL_IF_ONCE(expr, ...)                \
  do {                                                    \
    if (FLARE_UNLIKELY(expr)) {                           \
      FLARE_INTERNAL_DETAIL_LOG_ONCE(FATAL, __VA_ARGS__); \
    }                                                     \
  } while (0)

#define FLARE_DLOG_INFO(...)                                              \
  DLOG(INFO) << ::flare::internal::logging::FormatLog(__FILE__, __LINE__, \
                                                      __VA_ARGS__)
#define FLARE_DLOG_WARNING(...)                                              \
  DLOG(WARNING) << ::flare::internal::logging::FormatLog(__FILE__, __LINE__, \
                                                         __VA_ARGS__)
#define FLARE_DLOG_ERROR(...)                                              \
  DLOG(ERROR) << ::flare::internal::logging::FormatLog(__FILE__, __LINE__, \
                                                       __VA_ARGS__)
#define FLARE_DLOG_FATAL(...)                                              \
  DLOG(FATAL) << ::flare::internal::logging::FormatLog(__FILE__, __LINE__, \
                                                       __VA_ARGS__)

#define FLARE_DLOG_INFO_IF(expr, ...)                           \
  DLOG_IF(INFO, expr) << ::flare::internal::logging::FormatLog( \
      __FILE__, __LINE__, __VA_ARGS__)
#define FLARE_DLOG_WARNING_IF(expr, ...)                           \
  DLOG_IF(WARNING, FLARE_UNLIKELY(expr))                           \
      << ::flare::internal::logging::FormatLog(__FILE__, __LINE__, \
                                               __VA_ARGS__)
#define FLARE_DLOG_ERROR_IF(expr, ...)                             \
  DLOG_IF(ERROR, FLARE_UNLIKELY(expr))                             \
      << ::flare::internal::logging::FormatLog(__FILE__, __LINE__, \
                                               __VA_ARGS__)
#define FLARE_DLOG_FATAL_IF(expr, ...)                             \
  DLOG_IF(FATAL, FLARE_UNLIKELY(expr))                             \
      << ::flare::internal::logging::FormatLog(__FILE__, __LINE__, \
                                               __VA_ARGS__)

#define FLARE_DLOG_INFO_EVERY_N(N, ...)                           \
  DLOG_EVERY_N(INFO, N) << ::flare::internal::logging::FormatLog( \
      __FILE__, __LINE__, __VA_ARGS__)
#define FLARE_DLOG_WARNING_EVERY_N(N, ...)                           \
  DLOG_EVERY_N(WARNING, N) << ::flare::internal::logging::FormatLog( \
      __FILE__, __LINE__, __VA_ARGS__)
#define FLARE_DLOG_ERROR_EVERY_N(N, ...)                           \
  DLOG_EVERY_N(ERROR, N) << ::flare::internal::logging::FormatLog( \
      __FILE__, __LINE__, __VA_ARGS__)
#define FLARE_DLOG_FATAL_EVERY_N(N, ...)                           \
  DLOG_EVERY_N(FATAL, N) << ::flare::internal::logging::FormatLog( \
      __FILE__, __LINE__, __VA_ARGS__)

#ifndef NDEBUG
#define FLARE_DLOG_INFO_ONCE(...) \
  FLARE_INTERNAL_DETAIL_LOG_ONCE(INFO, __VA_ARGS__)
#define FLARE_DLOG_WARNING_ONCE(...) \
  FLARE_INTERNAL_DETAIL_LOG_ONCE(WARNING, __VA_ARGS__)
#define FLARE_DLOG_ERROR_ONCE(...) \
  FLARE_INTERNAL_DETAIL_LOG_ONCE(ERROR, __VA_ARGS__)
#define FLARE_DLOG_FATAL_ONCE(...) \
  FLARE_INTERNAL_DETAIL_LOG_ONCE(FATAL, __VA_ARGS__)
#else
// The expansion below is NOT a bug.
//
// FLARE_DLOG_XXX is expanded to "no-op" expression if `NDEBUG` is defined.
// Therefore, although `FLARE_DLOG_INFO_ONCE` doesn't behave in the same way as
// `FLARE_DLOG_INFO`, the end result it the same (nothing is ever evaluated).
//
// The reason why we expands to `FLARE_DLOG_XXX` instead of simply `(void)0` is
// that `FLARE_DLOG_XXX` does some trick (done by glog, to be precise) to avoid
// "unused variable" in a best-effort fashion, and we want to benefit from that
// here.
#define FLARE_DLOG_INFO_ONCE(...) FLARE_DLOG_INFO(__VA_ARGS__);
#define FLARE_DLOG_WARNING_ONCE(...) FLARE_DLOG_WARNING(__VA_ARGS__);
#define FLARE_DLOG_ERROR_ONCE(...) FLARE_DLOG_ERROR(__VA_ARGS__);
#define FLARE_DLOG_FATAL_ONCE(...) FLARE_DLOG_FATAL(__VA_ARGS__);
#endif

#define FLARE_PLOG_INFO(...)                                              \
  PLOG(INFO) << ::flare::internal::logging::FormatLog(__FILE__, __LINE__, \
                                                      __VA_ARGS__)
#define FLARE_PLOG_WARNING(...)                                              \
  PLOG(WARNING) << ::flare::internal::logging::FormatLog(__FILE__, __LINE__, \
                                                         __VA_ARGS__)
#define FLARE_PLOG_ERROR(...)                                              \
  PLOG(ERROR) << ::flare::internal::logging::FormatLog(__FILE__, __LINE__, \
                                                       __VA_ARGS__)
#define FLARE_PLOG_FATAL(...)                                              \
  PLOG(FATAL) << ::flare::internal::logging::FormatLog(__FILE__, __LINE__, \
                                                       __VA_ARGS__)

#define FLARE_PLOG_INFO_IF(expr, ...)                           \
  PLOG_IF(INFO, expr) << ::flare::internal::logging::FormatLog( \
      __FILE__, __LINE__, __VA_ARGS__)
#define FLARE_PLOG_WARNING_IF(expr, ...)                           \
  PLOG_IF(WARNING, FLARE_UNLIKELY(expr))                           \
      << ::flare::internal::logging::FormatLog(__FILE__, __LINE__, \
                                               __VA_ARGS__)
#define FLARE_PLOG_ERROR_IF(expr, ...)                             \
  PLOG_IF(ERROR, FLARE_UNLIKELY(expr))                             \
      << ::flare::internal::logging::FormatLog(__FILE__, __LINE__, \
                                               __VA_ARGS__)
#define FLARE_PLOG_FATAL_IF(expr, ...)                             \
  PLOG_IF(FATAL, FLARE_UNLIKELY(expr))                             \
      << ::flare::internal::logging::FormatLog(__FILE__, __LINE__, \
                                               __VA_ARGS__)

#ifdef _MSC_VER
#define FLARE_UNREACHABLE(...)                                              \
  do {                                                                      \
    LOG(FATAL) << "UNREACHABLE. "                                           \
               << ::flare::internal::logging::FormatLog(__FILE__, __LINE__, \
                                                        ##__VA_ARGS__);     \
  } while (0)
#define FLARE_NOT_IMPLEMENTED(...)                                          \
  do {                                                                      \
    LOG(FATAL) << "Not implemented. "                                       \
               << ::flare::internal::logging::FormatLog(__FILE__, __LINE__, \
                                                        ##__VA_ARGS__);     \
  } while (0)
#define FLARE_UNEXPECTED(...)                                               \
  do {                                                                      \
    LOG(FATAL) << "UNEXPECTED. "                                            \
               << ::flare::internal::logging::FormatLog(__FILE__, __LINE__, \
                                                        ##__VA_ARGS__);     \
  } while (0)
#else
#define FLARE_UNREACHABLE(...)                                                \
  do {                                                                        \
    [&]() __attribute__((noreturn, noinline, cold)) {                         \
      LOG(FATAL) << "UNREACHABLE. "                                           \
                 << ::flare::internal::logging::FormatLog(__FILE__, __LINE__, \
                                                          ##__VA_ARGS__);     \
      __builtin_unreachable();                                                \
    }                                                                         \
    ();                                                                       \
  } while (0)
#define FLARE_NOT_IMPLEMENTED(...)                                            \
  do {                                                                        \
    [&]() __attribute__((noreturn, noinline, cold)) {                         \
      LOG(FATAL) << "Not implemented. "                                       \
                 << ::flare::internal::logging::FormatLog(__FILE__, __LINE__, \
                                                          ##__VA_ARGS__);     \
      __builtin_unreachable();                                                \
    }                                                                         \
    ();                                                                       \
  } while (0)
#define FLARE_UNEXPECTED(...)                                                 \
  do {                                                                        \
    [&]() __attribute__((noreturn, noinline, cold)) {                         \
      LOG(FATAL) << "UNEXPECTED. "                                            \
                 << ::flare::internal::logging::FormatLog(__FILE__, __LINE__, \
                                                          ##__VA_ARGS__);     \
      __builtin_unreachable();                                                \
    }                                                                         \
    ();                                                                       \
  } while (0)
#endif

///////////////////////////////////////
// Implementation goes below.        //
///////////////////////////////////////

// C++ attribute won't work here.
//
// @sa: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=89640,
#define FLARE_INTERNAL_DETAIL_LOGGING_ATTRIBUTE_NOINLINE_COLD \
  __attribute__((noinline, cold))
#define FLARE_INTERNAL_DETAIL_LOGGING_ATTRIBUTE_NORETURN_NOINLINE_COLD \
  __attribute__((noreturn, noinline, cold))

namespace flare {

namespace internal::logging {

namespace details {

// Converts `value` to string. This is only used when `FormatLog` fails -- in
// this case we write arguments to log file for later investigation.
template <class T>
std::string ToString(const T& value) {
  return fmt::format("{}", value);
}

// Describes arguments to `FormatLog`.
std::string DescribeFormatArguments(const std::vector<std::string>& args);

}  // namespace details

// Prefix writer.
//
// Note that the implementation MAY NOT touch whatever has been in `to`. The
// implementation may only append its own prefix to `to`.
//
// Surely using `Function<...>` is superior here, but given that logging is a
// low level facility, bring in too many dependencies is unlikely a good idea.
using PrefixAppender = void(std::string* to);

// Install a new logging prefix provider.
void InstallPrefixProvider(PrefixAppender* writer);

// Call logging prefix providers to generate the prefix for log.
void WritePrefixTo(std::string* to);

// FOR INTERNAL USE ONLY.
//
// Logging prefix providers must be registered before `main` is entered to avoid
// potential race conditions. We provide this macro to accomplish this.
#define FLARE_INTERNAL_LOGGING_REGISTER_PREFIX_PROVIDER(priority, cb)     \
  [[gnu::constructor(priority + 101)]] static void FLARE_INTERNAL_PP_CAT( \
      flare_reserved_logging_prefix_provider_installer_, __COUNTER__)() { \
    ::flare::internal::logging::InstallPrefixProvider(cb);                \
  }

// Marked as noexcept. Throwing in formatting log is likely a programming
// error.
//
// Forwarding-ref does not get along well with packed field, so we use
// const-ref here.
template <class... Ts>
std::string FormatLog([[maybe_unused]] const char* file,
                      [[maybe_unused]] int line, const Ts&... args) noexcept {
  std::string result;

  WritePrefixTo(&result);
  if constexpr (sizeof...(Ts) != 0) {
    try {
      result += fmt::format(args...);
    } catch (const std::exception& xcpt) {
      // Presumably a wrong format string was provided?
      //
      // Don't panic here, aborting the whole program merely because of a
      // mal-formatted log message doesn't feel right.
      return fmt::format(
          "Failed to format log at [{}:{}] with arguments ({}): {}", file, line,
          details::DescribeFormatArguments({details::ToString(args)...}),
          xcpt.what());
    }
  }
  return result;
}

}  // namespace internal::logging
}  // namespace flare

// Clang 10 has not implemented P1381R1 yet, therefore the "cold lambda" trick
// won't work quite right if structured binding identifiers are accessed during
// evaluating log message.
#if defined(__clang__) && __clang__ <= 10

#define FLARE_INTERNAL_DETAIL_LOGGING_CHECK(expr, ...)                        \
  do {                                                                        \
    if (FLARE_UNLIKELY(!(expr))) {                                            \
      ::google::LogMessage(__FILE__, __LINE__, ::google::GLOG_FATAL).stream() \
          << "Check failed: " #expr " "                                       \
          << ::flare::internal::logging::FormatLog(__FILE__, __LINE__,        \
                                                   ##__VA_ARGS__);            \
      FLARE_UNREACHABLE();                                                    \
    }                                                                         \
  } while (0)

// `val` and `val2` are COPIED in the macro to avoid dangling reference. This
// can be SLOW.
//
// See comments on `FLARE_INTERNAL_DETAIL_LOGGING_CHECK_OP` provided for GCC /
// clang 10+ (see below) for discussion how can dangling reference occur.
#define FLARE_INTERNAL_DETAIL_LOGGING_CHECK_OP(name, op, val1, val2, ...)      \
  do {                                                                         \
    /* Copied to avoid dangling reference. SLOW. */                            \
    auto flare_anonymous_x = (val1);                                           \
    auto flare_anonymous_y = (val2);                                           \
    if (FLARE_UNLIKELY(!(flare_anonymous_x op flare_anonymous_y))) {           \
      ::google::LogMessageFatal(                                               \
          __FILE__, __LINE__,                                                  \
          ::google::CheckOpString(::google::MakeCheckOpString(                 \
              flare_anonymous_x, flare_anonymous_y, #val1 " " #op " " #val2))) \
              .stream()                                                        \
          << ::flare::internal::logging::FormatLog(__FILE__, __LINE__,         \
                                                   ##__VA_ARGS__);             \
      FLARE_UNREACHABLE();                                                     \
    }                                                                          \
  } while (0)

#define FLARE_INTERNAL_DETAIL_LOGGING_PCHECK(expr, ...)                      \
  do {                                                                       \
    if (FLARE_UNLIKELY(!(expr))) {                                           \
      ::google::ErrnoLogMessage(__FILE__, __LINE__, ::google::GLOG_FATAL, 0, \
                                &::google::LogMessage::SendToLog)            \
              .stream()                                                      \
          << "Check failed: " #expr " "                                      \
          << ::flare::internal::logging::FormatLog(__FILE__, __LINE__,       \
                                                   ##__VA_ARGS__);           \
      FLARE_UNREACHABLE();                                                   \
    }                                                                        \
  } while (0)

#define FLARE_INTERNAL_DETAIL_LOG_ONCE(Level, ...)                      \
  do {                                                                  \
    static std::atomic<bool> flare_anonymous_logged{false};             \
    if (FLARE_UNLIKELY(                                                 \
            !flare_anonymous_logged.load(std::memory_order_relaxed))) { \
      if (!flare_anonymous_logged.exchange(true)) {                     \
        LOG(Level) << ::flare::internal::logging::FormatLog(            \
            __FILE__, __LINE__, __VA_ARGS__);                           \
      }                                                                 \
    }                                                                   \
  } while (0)

#define FLARE_INTERNAL_DETAIL_LOG_EVERY_N(Level, N, ...)                  \
  do {                                                                    \
    /* Race here, as obvious. This is how glog has done, and I haven't */ \
    /* come up with a better idea yet (unless in trade of perf.). */      \
    static int flare_anonymous_logged_counter_mod_n = 0;                  \
    if (FLARE_UNLIKELY(++flare_anonymous_logged_counter_mod_n > N)) {     \
      flare_anonymous_logged_counter_mod_n -= N;                          \
      if (flare_anonymous_logged_counter_mod_n == 1)                      \
        google::LogMessage(__FILE__, __LINE__, google::GLOG_##Level, 0,   \
                           &google::LogMessage::SendToLog)                \
                .stream()                                                 \
            << ::flare::internal::logging::FormatLog(__FILE__, __LINE__,  \
                                                     __VA_ARGS__);        \
    }                                                                     \
  } while (0)

#else

#define FLARE_INTERNAL_DETAIL_LOGGING_CHECK(expr, ...)                       \
  do {                                                                       \
    if (FLARE_UNLIKELY(!(expr))) {                                           \
      /* Attribute `noreturn` is not applicable to lambda, unfortunately. */ \
      [&]() FLARE_INTERNAL_DETAIL_LOGGING_ATTRIBUTE_NORETURN_NOINLINE_COLD { \
        ::google::LogMessage(__FILE__, __LINE__, ::google::GLOG_FATAL)       \
                .stream()                                                    \
            << "Check failed: " #expr " "                                    \
            << ::flare::internal::logging::FormatLog(__FILE__, __LINE__,     \
                                                     ##__VA_ARGS__);         \
        FLARE_UNREACHABLE();                                                 \
      }();                                                                   \
    }                                                                        \
  } while (0)

// CAUTION: Do NOT use `auto&& x = (val1)` in the macro. Use IIFE instead (as
// shown below).
//
// The `auto&&` trick won't work if `std::vector{1, 2}[0]` is given `val1`. In
// this case `x` is a dangling reference as it refers to the (first) element of
// the already-destroyed temporary vector.
#define FLARE_INTERNAL_DETAIL_LOGGING_CHECK_OP(name, op, val1, val2, ...)    \
  [&](auto&& flare_anonymous_x, auto&& flare_anonymous_y) {                  \
    if (FLARE_UNLIKELY(!(flare_anonymous_x op flare_anonymous_y))) {         \
      [&]() FLARE_INTERNAL_DETAIL_LOGGING_ATTRIBUTE_NORETURN_NOINLINE_COLD { \
        ::google::LogMessageFatal(                                           \
            __FILE__, __LINE__,                                              \
            ::google::CheckOpString(::google::MakeCheckOpString(             \
                flare_anonymous_x, flare_anonymous_y,                        \
                #val1 " " #op " " #val2)))                                   \
                .stream()                                                    \
            << ::flare::internal::logging::FormatLog(__FILE__, __LINE__,     \
                                                     ##__VA_ARGS__);         \
        FLARE_UNREACHABLE();                                                 \
      }();                                                                   \
    }                                                                        \
  }((val1), (val2))

#define FLARE_INTERNAL_DETAIL_LOGGING_PCHECK(expr, ...)                        \
  do {                                                                         \
    if (FLARE_UNLIKELY(!(expr))) {                                             \
      [&]() FLARE_INTERNAL_DETAIL_LOGGING_ATTRIBUTE_NORETURN_NOINLINE_COLD {   \
        ::google::ErrnoLogMessage(__FILE__, __LINE__, ::google::GLOG_FATAL, 0, \
                                  &::google::LogMessage::SendToLog)            \
                .stream()                                                      \
            << "Check failed: " #expr " "                                      \
            << ::flare::internal::logging::FormatLog(__FILE__, __LINE__,       \
                                                     ##__VA_ARGS__);           \
        FLARE_UNREACHABLE();                                                   \
      }();                                                                     \
    }                                                                          \
  } while (0)

#define FLARE_INTERNAL_DETAIL_LOG_ONCE(Level, ...)                      \
  do {                                                                  \
    static std::atomic<bool> flare_anonymous_logged{false};             \
    if (FLARE_UNLIKELY(                                                 \
            !flare_anonymous_logged.load(std::memory_order_relaxed))) { \
      [&]() FLARE_INTERNAL_DETAIL_LOGGING_ATTRIBUTE_NOINLINE_COLD {     \
        if (!flare_anonymous_logged.exchange(true)) {                   \
          LOG(Level) << ::flare::internal::logging::FormatLog(          \
              __FILE__, __LINE__, __VA_ARGS__);                         \
        }                                                               \
      }();                                                              \
    }                                                                   \
  } while (0)

#define FLARE_INTERNAL_DETAIL_LOG_EVERY_N(Level, N, ...)                   \
  do {                                                                     \
    /* Race here, as obvious. This is how glog has done, and I haven't */  \
    /* come up with a better idea yet (unless in trade of perf.). */       \
    static int flare_anonymous_logged_counter_mod_n = 0;                   \
    if (FLARE_UNLIKELY(++flare_anonymous_logged_counter_mod_n > N)) {      \
      [&]() FLARE_INTERNAL_DETAIL_LOGGING_ATTRIBUTE_NOINLINE_COLD {        \
        flare_anonymous_logged_counter_mod_n -= N;                         \
        if (flare_anonymous_logged_counter_mod_n == 1)                     \
          google::LogMessage(__FILE__, __LINE__, google::GLOG_##Level, 0,  \
                             &google::LogMessage::SendToLog)               \
                  .stream()                                                \
              << ::flare::internal::logging::FormatLog(__FILE__, __LINE__, \
                                                       __VA_ARGS__);       \
      }();                                                                 \
    }                                                                      \
  } while (0)

#endif

#define FLARE_INTERNAL_DETAIL_LOG_IF_EVERY_N(expr, Level, N, ...) \
  do {                                                            \
    if (expr) {                                                   \
      FLARE_INTERNAL_DETAIL_LOG_EVERY_N(Level, N, __VA_ARGS__);   \
    }                                                             \
  } while (0)

#endif  // FLARE_BASE_INTERNAL_LOGGING_H_
