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
#include <unordered_set>
#include <utility>

#include "gflags/gflags.h"
#include "opentracing/ext/tags.h"

#include "flare/base/exposed_var.h"
#include "flare/base/internal/cpu.h"
#include "flare/base/internal/dpc.h"
#include "flare/base/internal/hash_map.h"
#include "flare/base/never_destroyed.h"
#include "flare/base/overloaded.h"
#include "flare/base/random.h"
#include "flare/base/thread/thread_cached.h"
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

struct StringViewHash {
  std::size_t operator()(const opentracing::string_view& s) const noexcept {
    return std::hash<std::string_view>()(std::string_view(s.data(), s.size()));
  }
};

bool IsStandardTag(const opentracing::string_view& tag) {
  // TODO(luobogao): We need a better way to enumerate standard tags.
  static const std::unordered_set<opentracing::string_view, StringViewHash>
      kStandardTags = {opentracing::ext::span_kind,
                       opentracing::ext::span_kind_rpc_client,
                       opentracing::ext::span_kind_rpc_server,
                       opentracing::ext::error,
                       opentracing::ext::component,
                       opentracing::ext::sampling_priority,
                       opentracing::ext::peer_service,
                       opentracing::ext::peer_hostname,
                       opentracing::ext::peer_address,
                       opentracing::ext::peer_host_ipv4,
                       opentracing::ext::peer_host_ipv6,
                       opentracing::ext::peer_port,
                       opentracing::ext::http_url,
                       opentracing::ext::http_method,
                       opentracing::ext::http_status_code,
                       opentracing::ext::database_instance,
                       opentracing::ext::database_statement,
                       opentracing::ext::database_type,
                       opentracing::ext::database_user,
                       opentracing::ext::message_bus_destination};
  return kStandardTags.find(tag) != kStandardTags.end();
}

bool IsFrameworkTag(const opentracing::string_view& tag) {
  return StartsWith(std::string_view(tag.data(), tag.size()), "flare.");
}

}  // namespace detail

void QuickerSpan::FlushBufferedOps() {
  for (auto&& e : buffered_ops_) {
    opentracing::Value translated;

    std::visit(
        // Functors are evaluated now, otherwise the value is returned as-is.
        Overloaded{
            // Not using `const Function<std::string()>` here, otherwise the
            // catch-all below would take precedence in overload resolution.
            [&](Function<std::string()>& f) { translated = f(); },
            [&](std::string_view& sv) { translated = std::string(sv); },
            [&](auto&& v) { translated = v; }},
        e.value);
    if (e.type == Operation::FrameworkTag) {
      ops_->provider_->SetFrameworkTag(span_.get(), e.GetKey(), translated);
    } else if (e.type == Operation::StandardTag ||
               e.type == Operation::UserTag) {
      span_->SetTag(e.GetKey(), translated);
    } else {
      FLARE_CHECK(e.type == Operation::Log);
      span_->Log({std::pair(e.GetKey(), translated)});
    }
  }
}

void QuickerSpan::ReportViaDpc() {
  auto cb = [span = std::move(span_),
             finished_at = ReadSteadyClock()]() mutable {
    // No we cannot simply call `opentracing::Span::Finish()` here as by the
    // time we're called (asynchronously via DPC), an undetermined time period
    // has passed. Because `Finish()` internally captures current timestamp as
    // "finishing timestamp", it's likely wrong. Therefore, here we use the time
    // point we recorded when `ReportViaDpc()` was called to finish the span.
    opentracing::FinishSpanOptions options;
    options.finish_steady_timestamp = finished_at;
    span->FinishWithOptions(options);
    reported_spans->Add(1);

    // Any sane implementation shouldn't report twice (implicitly via `span`'s
    // dtor.)
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
