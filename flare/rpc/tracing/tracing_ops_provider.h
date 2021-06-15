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
#include <string>

#include "opentracing-cpp/span.h"
#include "opentracing-cpp/tracer.h"

#include "flare/base/dependency_registry.h"
#include "flare/base/function.h"
#include "flare/base/internal/macro.h"

namespace flare::tracing {

// `TracingOps` uses this class to implement its job.
//
// For more detail about `TracingOps`, see its class header comment.
class TracingOpsProvider {
 public:
  virtual ~TracingOpsProvider() = default;

  virtual std::unique_ptr<opentracing::Span> StartSpanWithOptions(
      opentracing::string_view operation_name,
      const opentracing::StartSpanOptions& options) const noexcept = 0;

  virtual void SetFrameworkTag(opentracing::Span* span,
                               opentracing::string_view key,
                               const opentracing::Value& value) = 0;

  virtual bool Inject(const opentracing::SpanContext& sc,
                      std::string* out) const = 0;

  virtual opentracing::expected<std::unique_ptr<opentracing::SpanContext>>
  Extract(const std::string& in) const = 0;

  // Check if `span` is sampled. If not, buffered tags are discarded without
  // flushing into `span` (for perf. reasons).
  //
  // Besides, for non-sampled spans, they're always destroyed synchronously, as
  // we expect destroying non-sampled spans should be lightweight (If this is
  // not your case, let us know and see if this interface should be refined.).
  virtual bool IsSampled(const opentracing::Span& span) const noexcept = 0;
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
    const std::string_view& provider, const TracingOpsProviderOptions& options);

}  // namespace flare::tracing

FLARE_DECLARE_CLASS_DEPENDENCY_REGISTRY(
    flare_tracing_tracer_ops_provider_factory_registry,
    flare::tracing::TracingOpsProvider,
    flare::tracing::TracingOpsProviderOptions);

#define FLARE_TRACING_REGISTER_TRACER_OPS_PROVIDER_FACTORY(Name, Factory) \
  FLARE_REGISTER_CLASS_DEPENDENCY_FACTORY(                                \
      flare_tracing_tracer_ops_provider_factory_registry, Name, Factory)

#endif  // FLARE_RPC_TRACING_TRACING_OPS_PROVIDER_H_
