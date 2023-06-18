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

#ifndef FLARE_RPC_TRACING_TRACING_OPS_H_
#define FLARE_RPC_TRACING_TRACING_OPS_H_

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "gtest/gtest_prod.h"
#include "opentracing-cpp/span.h"
#include "opentracing-cpp/tracer.h"
#include "opentracing/ext/tags.h"

#include "flare/base/chrono.h"
#include "flare/base/maybe_owning.h"
#include "flare/base/never_destroyed.h"
#include "flare/base/string.h"
#include "flare/rpc/internal/sampler.h"
#include "flare/rpc/tracing/tracing_ops_provider.h"

namespace flare::tracing {

namespace detail {

bool IsStandardTag(const opentracing::string_view& tag);
bool IsFrameworkTag(const opentracing::string_view& tag);

}  // namespace detail

class TracingOps;

// A wrapper for `opentracing::Span` that is... quicker.
class QuickerSpan {
 public:
  // The default-constructed one is a "noop" span (i.e., all its methods are
  // effectively do nothing.).
  QuickerSpan() = default;

  explicit QuickerSpan(TracingOps* ops, std::unique_ptr<opentracing::Span> span)
      : ops_(ops), span_(std::move(span)) {}

  ~QuickerSpan() {
    FLARE_CHECK(!Tracing(),
                "You should `Report()` the span before destroying it.");
  }

  // Movable but not copyable.
  QuickerSpan(QuickerSpan&&) = default;
  QuickerSpan& operator=(QuickerSpan&&) = default;

  // Setting tags should be multi-thread safe (guaranteed by `Span`'s
  // implementation.)

  // Set standard tag on span.
  //
  // Only tags defined in `opentracing::ext::` should be used here. These tags
  // are forwarded to `span` without further translation.
  template <class V>
  void SetStandardTag(const opentracing::string_view&, V&& value);

  // Tags defined by flare framework should be translated by the provider before
  // setting it into span.
  //
  // @sa: `framework_tags.h`
  template <class V>
  void SetFrameworkTag(const opentracing::string_view&, V&& value);

  // User tags are passed through. It's recommended to use a distinct prefix for
  // user tags to avoid name collision.
  template <class V>
  void SetUserTag(std::string key, V&& value);

  // Append a log item to the trace.
  void Log(std::string key, std::string value);

  // Baggage items, AFAICS, are only sensible for framework's use. As we don't
  // use baggage items to pass context across service boundary (at least for
  // now), we don't expose it.
  //
  // void AddBaggageItem(...);

  // Flush any tags buffered tags and report it to the provider.
  //
  // If the provider's `SetTag` is way too slow (e.g., tjg provider is likely
  // not very performance), it might be beneficial to buffer KV pairs ourself
  // and add them later when the span is indeed going to be reported.
  void Report();

  // You'll need this to derive your own client-side span.
  //
  // `nullptr` is returned if `Tracing()` does not hold.
  const opentracing::SpanContext* SpanContext() const noexcept {
    return Tracing() ? &span_->context() : nullptr;
  }

  // Serialize span context to byte stream, which is (normally) transmitted to
  // another peer later.
  bool WriteContextTo(std::string* serialized);

  // Check if the span should be unconditionally reported. Note that this flag
  // is backward propagated all the way up to the top-most RPC caller.
  bool IsForciblySampled() const noexcept { return forcibly_sampled_; }

  // If set, the span will be reported unconditionally.
  //
  // This is merely a hint, and the implementation is free to ignore it.
  void AdviseForciblySampled() noexcept {
    if (Tracing() && IsForceSampleAllowed()) {
      SetForciblySampled();
    }  // Ignored otherwise.
  }

  // Same as `AdviseForciblySampled()` but this one is mandatory and must be
  // respected.
  //
  // Thread-safe.
  void SetForciblySampled() noexcept {
    if (Tracing()) {  // `IsForceSampleAllowed()` is not consulted.
      forcibly_sampled_ = true;
    }
  }

  // Returns whether we're tracing the span.
  bool Tracing() const noexcept { return !!span_; }

 private:
  FRIEND_TEST(TracingOps, Noop);
  FRIEND_TEST(TracingOps, DummyProvider);

  enum class Operation { StandardTag, FrameworkTag, UserTag, Log };

  inline static constexpr struct standard_tag_t {
    constexpr explicit standard_tag_t() = default;
  } standard_tag{};
  inline static constexpr struct framework_tag_t {
    constexpr explicit framework_tag_t() = default;
  } framework_tag{};
  inline static constexpr struct user_tag_t {
    constexpr explicit user_tag_t() = default;
  } user_tag{};
  inline static constexpr struct log_t {
    constexpr explicit log_t() = default;
  } log{};

  struct BufferedOp {
    using QuickerValue =
        std::variant<bool, std::int32_t, std::uint32_t, std::int64_t,
                     std::uint64_t, std::string, Function<std::string()>>;

    Operation type;

    // For user tags or logs, `key`'s lifetime is not guaranteed, so we make a
    // copy here. Not applicable to non-user tags.
    //
    // This is an implementation detail, don't touch it.
    std::variant<opentracing::string_view, std::string> key;
    QuickerValue value;

    template <class V>
    BufferedOp(standard_tag_t, const opentracing::string_view& key, V&& value)
        : type(Operation::StandardTag),
          key(key),
          value(std::forward<V>(value)) {}
    template <class V>
    BufferedOp(framework_tag_t, const opentracing::string_view& key, V&& value)
        : type(Operation::FrameworkTag),
          key(key),
          value(std::forward<V>(value)) {}
    template <class V>
    BufferedOp(user_tag_t, std::string key_str, V&& value)
        : type(Operation::UserTag), value(std::forward<V>(value)) {
      key = std::move(key_str);
    }
    BufferedOp(log_t, std::string key_str, std::string value)
        : type(Operation::Log), value(std::move(value)) {
      key = std::move(key_str);
    }

    opentracing::string_view GetKey() const noexcept {
      return key.index() == 0 ? std::get<0>(key)
                              : opentracing::string_view(std::get<1>(key));
    }
  };

  // Flush buffered tags / logs into `span_`.
  void FlushBufferedOps();

  // Report the span asynchronously.
  void ReportViaDpc();

  // Throttles "unplanned" trace report. At most 1 unplanned report per thread
  // is allowed every 1s.
  static bool IsForceSampleAllowed() {
    // We allow at most 1 sample to be forcibly sampled per 100ms.
    static NeverDestroyed<rpc::detail::LargeIntervalSampler> sampler(
        std::chrono::milliseconds(100));
    return sampler->Sample();
  }

 private:
  TracingOps* ops_;
  std::unique_ptr<opentracing::Span> span_;
  std::vector<BufferedOp> buffered_ops_;
  // FIXME: Thread-safety. (`std::atomic_ref` should come to rescue.) To make
  // the whole class movable, we cannot simply use `std::atomic<T>` here.
  bool forcibly_sampled_{false};
};

// This class implements (and hides implementation detail of) all operations
// required by distributed tracing (given the the concrete implementation
// conforms to OpenTracing standard.)
//
// To be fair, `opentracing-cpp` already implements almost *everything* we need
// (it even comes with a "noop" tracer which is "reimplemented" here), except
// that performance is not seriously guaranteed. (The "noop" tracer does state
// that it comes with minimal perf. overhead, but that's still relatively large
// compared to implementation here.)
//
// Here we "reinvent the wheel" for:
//
// - Better performance when distributed tracing is NOT enabled.
// - Unified interface for supporting non-(opentracing-)standard tags.
//
// Note that you need to execute a barrier on DPC to wait for pending DPCs
// posted by `QuickerSpan::Report()`.
class TracingOps {
 public:
  // For no-op behavior, pass `nullptr` to `provider`.
  explicit TracingOps(std::unique_ptr<TracingOpsProvider> provider)
      : provider_(std::move(provider)) {}

  // Start a new span, `apply_opts` is called ONLY IF we're using a non-noop
  // tracer.
  //
  // DO NOT RELY ON `apply_opts` BEING EVALUATED UNCONDITIONALLY.
  //
  // It's recommended NOT to apply tags in `apply_opts`, as some
  // implementation's `SetTag` is slow. By calling `QuickerSpan::SetXxxTag`
  // instead, the framework can buffer the calls to `SetXxxTag` until it's
  // absolutely necessary (and eliminate the call completely if possible),
  // therefore boosting performance. (However, some tracing provider DO require
  // tags be set in `apply_opts`, consult documentation of provider you use for
  // details.)
  template <class F>
  QuickerSpan StartSpanWithLazyOptions(std::string_view operation_name,
                                       F&& apply_opts) {
    if (Tracing()) {
      opentracing::StartSpanOptions options = {
          .start_system_timestamp = ReadSystemClock(),
          .start_steady_timestamp = ReadSteadyClock()};
      std::forward<F>(apply_opts)([&](auto&& opt) { opt.Apply(options); });

      // OpenTracing uses its own `string_view`, which by the of writing, cannot
      // be constructed from `std::string_view` implicitly.
      opentracing::string_view onsv(operation_name.data(),
                                    operation_name.size());
      return QuickerSpan{this, provider_->StartSpanWithOptions(onsv, options)};
    }

    return QuickerSpan{nullptr /* Doesn't matter. */, nullptr};
  }

  // Deserialize span context from byte stream.
  opentracing::expected<std::unique_ptr<opentracing::SpanContext>>
  ParseSpanContextFrom(const std::string& serialized) {
    if (Tracing()) {
      return provider_->Extract(serialized);
    }
    return std::unique_ptr<opentracing::SpanContext>();
  }

 private:
  friend class QuickerSpan;

  bool Tracing() const noexcept { return !!provider_; }
  TracingOpsProvider* GetProvider() const noexcept { return provider_.get(); }

 private:
  std::unique_ptr<TracingOpsProvider> provider_;
};

// Get `TracingOps` for distributed tracing.
TracingOps* GetTracingOps(std::string_view service);

////////////////////////////////////////
// Implementation goes below.         //
////////////////////////////////////////

template <class V>
void QuickerSpan::SetStandardTag(const opentracing::string_view& key,
                                 V&& value) {
  FLARE_DCHECK(detail::IsStandardTag(key));
  if (FLARE_UNLIKELY(Tracing())) {
    buffered_ops_.emplace_back(standard_tag, key, std::forward<V>(value));
  }  // Nothing otherwise.
}

template <class V>
void QuickerSpan::SetFrameworkTag(const opentracing::string_view& key,
                                  V&& value) {
  FLARE_DCHECK(detail::IsFrameworkTag(key));
  if (FLARE_UNLIKELY(Tracing())) {
    buffered_ops_.emplace_back(framework_tag, key, std::forward<V>(value));
  }  // Nothing otherwise.
}

template <class V>
void QuickerSpan::SetUserTag(std::string key, V&& value) {
  FLARE_DCHECK(!detail::IsStandardTag(key) && !detail::IsFrameworkTag(key));
  if (FLARE_UNLIKELY(Tracing())) {
    buffered_ops_.emplace_back(user_tag, std::move(key),
                               std::forward<V>(value));
  }
}

inline void QuickerSpan::Log(std::string key, std::string value) {
  if (FLARE_UNLIKELY(Tracing())) {
    buffered_ops_.emplace_back(log, key, std::move(value));
  }
}

inline bool QuickerSpan::WriteContextTo(std::string* serialized) {
  if (FLARE_UNLIKELY(Tracing())) {
    return ops_->provider_->Inject(span_->context(), serialized);
  }
  serialized->clear();  // Nothing to inject otherwise.
  return true;
}

inline void QuickerSpan::Report() {
  if (FLARE_UNLIKELY(Tracing())) {
    if (forcibly_sampled_) {
      // Any sane implementation should treat the trace as sampled afterwards.
      span_->SetTag(opentracing::ext::sampling_priority, 1);
    }
    if (ops_->GetProvider()->IsSampled(*span_)) {
      FlushBufferedOps();  // Flushing buffered ops is done only when sampled.
      ReportViaDpc();
    } else {
      // If not sampled, finishing the span should be relatively cheap.
      //
      // FIXME: `Span`'s destructor unconditionally calls `steady_clock::now()`,
      // and that hurts performance (@sa: `doc/timestamps.md`.)
      span_ = nullptr;  // Finishes the span implicitly.
    }
  }  // Nothing to do otherwise.
}

}  // namespace flare::tracing

#endif  // FLARE_RPC_TRACING_TRACING_OPS_H_
