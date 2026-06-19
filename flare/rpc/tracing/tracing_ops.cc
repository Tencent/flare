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

#include "flare/rpc/tracing/tracing_ops.h"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "gflags/gflags.h"
#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/trace/span_metadata.h"  // `EndSpanOptions`

#include "flare/base/exposed_var.h"
#include "flare/base/internal/cpu.h"
#include "flare/base/internal/dpc.h"
#include "flare/base/internal/hash_map.h"
#include "flare/base/never_destroyed.h"
#include "flare/base/overloaded.h"
#include "flare/base/random.h"
#include "flare/base/string.h"
#include "flare/base/thread/thread_cached.h"
#include "flare/rpc/tracing/string_view_interop.h"
#include "flare/rpc/tracing/tracing_ops_provider.h"

DEFINE_string(flare_tracing_provider, "",
              "Distributed tracing provider. Leaving it empty disables tracing "
              "completely. Other choices are: 'tjg'.");

namespace flare::tracing {

namespace {

ExposedCounter<std::uint64_t> reported_spans("flare/rpc/reported_spans");

std::unique_ptr<TracingOps> MakeTracingOps(
    const std::string& provider,
    const TracingOpsProviderOptions& prov_options) {
  if (prov_options.service.empty()) {
    FLARE_LOG_WARNING_ONCE(
        "Creating tracer with empty service name. Although some implementation "
        "supports this, it's not recommended and you should provide a service "
        "name via `Server::Options`.");
  }
  if (provider.empty()) {
    return std::make_unique<TracingOps>(nullptr);
  }
  return std::make_unique<TracingOps>(
      MakeTracingOpsProvider(provider, prov_options));
}

}  // namespace

namespace detail {

bool IsStandardTag(otel::nostd::string_view tag) {
  // TODO(luobogao): We need a better way to enumerate standard tags.
  //
  // These are the conventional (OpenTracing-era) tag names flare still
  // recognizes. Note `span.kind` is no longer a tag -- it's carried as
  // `SpanStartOptions::kind`.
  static const std::unordered_set<std::string_view> kStandardTags = {
      "error",
      "component",
      "sampling_priority",
      "peer.service",
      "peer.hostname",
      "peer.address",
      "peer.ipv4",
      "peer.ipv6",
      "peer.port",
      "http.url",
      "http.method",
      "http.status_code",
      "db.instance",
      "db.statement",
      "db.type",
      "db.user",
      "message_bus.destination",
  };
  return kStandardTags.find(ToStd(tag)) != kStandardTags.end();
}

bool IsFrameworkTag(otel::nostd::string_view tag) {
  return StartsWith(ToStd(tag), "flare.");
}

}  // namespace detail

void QuickerSpan::FlushBufferedOps() {
  for (auto&& e : buffered_ops_) {
    // `AttributeValue` is a non-owning view; `scratch` backs any string it
    // refers to and must outlive the `Set*`/`AddEvent` call below. (For the
    // buffered-`std::string` case the storage in `e.value` is itself stable
    // through this iteration.)
    std::string scratch;
    otel::common::AttributeValue attr = std::visit(
        // Functors are evaluated now, otherwise the value is returned as-is.
        Overloaded{
            // Not using `const Function<std::string()>` here, otherwise the
            // catch-all below would take precedence in overload resolution.
            [&](Function<std::string()>& f) -> otel::common::AttributeValue {
              scratch = f();
              return otel::nostd::string_view(scratch.data(), scratch.size());
            },
            [&](std::string& s) -> otel::common::AttributeValue {
              return otel::nostd::string_view(s.data(), s.size());
            },
            [&](auto&& v) -> otel::common::AttributeValue { return v; }},
        e.value);

    if (e.type == Operation::FrameworkTag) {
      ops_->provider_->SetFrameworkTag(span_.get(), e.GetKey(), attr);
    } else if (e.type == Operation::StandardTag ||
               e.type == Operation::UserTag) {
      span_->SetAttribute(e.GetKey(), attr);
    } else {
      FLARE_CHECK(e.type == Operation::Log);
      // OpenTelemetry has no opentracing-style "log fields on span"; the
      // closest is an event. The flare log key becomes the event name and the
      // value an attribute. Note each flare log is its own event, unlike
      // opentracing which grouped multiple fields under one timestamped record.
      //
      // flare permits an empty log key (single-arg `AddTracingLog(value)`);
      // give those a non-empty event name so backends that group / display by
      // event name don't render a blank one.
      auto event_name = e.GetKey();
      span_->AddEvent(
          event_name.empty() ? otel::nostd::string_view("log") : event_name,
          {{"value", attr}});
    }
  }
}

void QuickerSpan::ReportViaDpc() {
  auto cb = [span = std::move(span_),
             finished_at = ReadSteadyClock()]() mutable {
    // No we cannot simply call `Span::End()` with default options here as by
    // the time we're called (asynchronously via DPC), an undetermined time
    // period has passed and `End()` would otherwise capture "now" as the finish
    // time. Use the time point recorded when `ReportViaDpc()` was called
    // instead.
    otel::trace::EndSpanOptions options;
    options.end_steady_time = otel::common::SteadyTimestamp(finished_at);
    span->End(options);
    reported_spans->Add(1);

    // Any sane implementation shouldn't report twice (the API contract is that
    // a `Span`'s dtor does not implicitly `End()`.)
  };
  internal::QueueDpc(std::move(cb));

  FLARE_CHECK(!Tracing());
}

TracingOps* GetTracingOps(std::string_view service) {
  // `std::shared_ptr<T>` is used below because `ThreadCached` requires `T` to
  // be `CopyConstructible`.
  //
  // Don't worry, we don't actually copy that `std::shared_ptr<T>` much. It's
  // only copied when we need to update the map (which is rare).
  static NeverDestroyed<
      ThreadCached<internal::HashMap<std::string, std::shared_ptr<TracingOps>>>>
      tracing_ops;

  auto ptr = tracing_ops->NonIdempotentGet().TryGet(service);
  if (FLARE_UNLIKELY(!ptr)) {
    static NeverDestroyed<std::mutex> create_lock;
    std::scoped_lock _(*create_lock);

    if (!tracing_ops->NonIdempotentGet().TryGet(service)) {  // DCLP.
      // It's indeed not there, let's create one and update the global map.
      //
      // Because we've grabbed `create_lock`, on one else can be contending
      // with us.
      auto ops = MakeTracingOps(
          FLAGS_flare_tracing_provider,
          TracingOpsProviderOptions{.service = std::string(service)});

      // Make a copy, update it, and store it back (all with lock held).
      auto copy = tracing_ops->NonIdempotentGet();
      copy[std::string(service)] = std::move(ops);
      tracing_ops->Emplace(std::move(copy));
    }

    ptr = tracing_ops->NonIdempotentGet().TryGet(service);
  }
  return ptr->get();
}

}  // namespace flare::tracing
