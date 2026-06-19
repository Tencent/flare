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

#ifndef FLARE_RPC_TRACING_TRACING_OPS_PROVIDER_H_
#define FLARE_RPC_TRACING_TRACING_OPS_PROVIDER_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/trace/span.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/span_metadata.h"  // `SpanKind`

#include "flare/base/dependency_registry.h"
#include "flare/base/function.h"
#include "flare/base/internal/macro.h"

namespace flare::tracing {

namespace otel = opentelemetry;

// Options for starting a span.
//
// This replaces `opentracing::StartSpanOptions`: OpenTelemetry has no
// `ChildOf`-style option objects, so the framework collects the parent context,
// span kind, timestamps, and any tags a provider needs at start-time here. The
// provider turns this into an `opentelemetry::trace::StartSpanOptions` plus the
// initial attribute set.
struct SpanStartOptions {
  // A default-constructed (invalid) `SpanContext` means "this is a root span".
  // `SpanContext` is a value type in OpenTelemetry, not a heap pointer.
  otel::trace::SpanContext parent = otel::trace::SpanContext::GetInvalid();

  otel::trace::SpanKind kind = otel::trace::SpanKind::kInternal;

  otel::common::SystemTimestamp start_system_timestamp;
  otel::common::SteadyTimestamp start_steady_timestamp;

  // Some providers require certain tags (e.g. span kind) be present at span
  // start rather than set later. They're carried here as (key, value) pairs.
  // The keys must outlive the `StartSpanWithOptions` call.
  std::vector<std::pair<otel::nostd::string_view, otel::common::AttributeValue>>
      attributes;
};

// `TracingOps` uses this class to implement its job.
//
// For more detail about `TracingOps`, see its class header comment.
class TracingOpsProvider {
 public:
  virtual ~TracingOpsProvider() = default;

  virtual otel::nostd::shared_ptr<otel::trace::Span> StartSpanWithOptions(
      otel::nostd::string_view operation_name,
      const SpanStartOptions& options) const noexcept = 0;

  // Translate the flare framework tag `key` to the name(s) recognized by the
  // provider's own implementation and set it onto `span`.
  virtual void SetFrameworkTag(otel::trace::Span* span,
                               otel::nostd::string_view key,
                               const otel::common::AttributeValue& value) = 0;

  // Serialize `sc` (a value type) into `out`. The wire format is opaque to the
  // framework -- the provider owns it (@sa: `rpc_meta.proto`'s
  // `tracing_context`).
  virtual bool Inject(const otel::trace::SpanContext& sc,
                      std::string* out) const = 0;

  // Inverse of `Inject`. Returns `std::nullopt` if `in` carries no (parseable)
  // context.
  virtual std::optional<otel::trace::SpanContext> Extract(
      const std::string& in) const = 0;

  // Check if `span` is sampled. If not, buffered tags are discarded without
  // flushing into `span` (for perf. reasons).
  //
  // Besides, for non-sampled spans, they're always destroyed synchronously, as
  // we expect destroying non-sampled spans should be lightweight (If this is
  // not your case, let us know and see if this interface should be refined.).
  virtual bool IsSampled(const otel::trace::Span& span) const noexcept = 0;
};

// The framework passes necessary information to `TracingOpsProvider` via this
// option structure.
struct TracingOpsProviderOptions {
  std::string service;
  std::string host;  // Host name, not necessarily IP. TODO(luobogao): Fill it.
};

// Factory of `TracingOps`.
using TracingOpsProviderFactory = Function<std::unique_ptr<TracingOpsProvider>(
    const TracingOpsProviderOptions&)>;

// Rest of the framework uses this method to create `TracingOps` instance.
std::unique_ptr<TracingOpsProvider> MakeTracingOpsProvider(
    std::string_view provider, const TracingOpsProviderOptions& options);

}  // namespace flare::tracing

FLARE_DECLARE_CLASS_DEPENDENCY_REGISTRY(
    flare_tracing_tracer_ops_provider_factory_registry,
    flare::tracing::TracingOpsProvider,
    flare::tracing::TracingOpsProviderOptions);

#define FLARE_TRACING_REGISTER_TRACER_OPS_PROVIDER_FACTORY(Name, Factory) \
  FLARE_REGISTER_CLASS_DEPENDENCY_FACTORY(                                \
      flare_tracing_tracer_ops_provider_factory_registry, Name, Factory)

#endif  // FLARE_RPC_TRACING_TRACING_OPS_PROVIDER_H_
