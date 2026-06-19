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

#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "gflags/gflags.h"
#include "gmock/gmock-matchers.h"
#include "gtest/gtest.h"
#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/common/key_value_iterable.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/trace/span.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/span_metadata.h"

#include "flare/fiber/this_fiber.h"
#include "flare/rpc/rpc_channel.h"
#include "flare/rpc/rpc_client_controller.h"
#include "flare/rpc/rpc_server_controller.h"
#include "flare/rpc/server.h"
#include "flare/rpc/tracing/framework_tags.h"
#include "flare/rpc/tracing/string_view_interop.h"
#include "flare/rpc/tracing/tracing_ops_provider.h"
#include "flare/testing/echo_service.flare.pb.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"
#include "flare/testing/relay_service.flare.pb.h"

using namespace std::literals;

DECLARE_string(flare_tracing_provider);
DECLARE_bool(flare_rpc_start_new_trace_on_missing);

namespace flare::tracing {

namespace {

std::string AttrToString(const otel::common::AttributeValue& v) {
  return otel::nostd::visit(
      [](auto&& x) -> std::string {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, otel::nostd::string_view>) {
          return std::string(x.data(), x.size());
        } else if constexpr (std::is_same_v<T, const char*>) {
          return std::string(x);
        } else if constexpr (std::is_same_v<T, bool>) {
          return x ? "true" : "false";
        } else if constexpr (std::is_arithmetic_v<T>) {
          return std::to_string(x);
        } else {
          return {};  // `span<>` alternatives, not used here.
        }
      },
      v);
}

// The span kind is a start-time property in OpenTelemetry (not a tag), but the
// test still wants to observe it, so the dummy provider records it under this
// key.
constexpr auto kSpanKind = "span.kind";

}  // namespace

struct MaterializedSpan {
  std::string method;
  std::vector<std::pair<std::string, std::string>> tags, logs;
};

std::mutex lock;
std::vector<MaterializedSpan> reported_spans;

// A recording fake span. (OpenTelemetry's bundled noop span discards
// everything, so it can't be asserted on.)
class DummySpan : public otel::trace::Span {
 public:
  void SetAttribute(
      otel::nostd::string_view key,
      const otel::common::AttributeValue& value) noexcept override {
    tags_.push_back({std::string(key.data(), key.size()), AttrToString(value)});
  }

  void AddEvent(otel::nostd::string_view) noexcept override {}
  void AddEvent(otel::nostd::string_view,
                otel::common::SystemTimestamp) noexcept override {}
  void AddEvent(
      otel::nostd::string_view name, otel::common::SystemTimestamp,
      const otel::common::KeyValueIterable& attributes) noexcept override {
    attributes.ForEachKeyValue(
        [&](otel::nostd::string_view,
            otel::common::AttributeValue value) noexcept {
          logs_.push_back(
              {std::string(name.data(), name.size()), AttrToString(value)});
          return true;
        });
  }

  void SetStatus(otel::trace::StatusCode,
                 otel::nostd::string_view) noexcept override {}
  void UpdateName(otel::nostd::string_view name) noexcept override {
    op_name_ = std::string(name.data(), name.size());
  }
  void End(const otel::trace::EndSpanOptions&) noexcept override {
    MaterializedSpan ms = {.method = op_name_, .tags = tags_, .logs = logs_};
    std::scoped_lock _(lock);
    reported_spans.push_back(ms);
  }
  otel::trace::SpanContext GetContext() const noexcept override {
    return otel::trace::SpanContext::GetInvalid();
  }
  bool IsRecording() const noexcept override { return true; }

  void set_op_name(std::string name) { op_name_ = std::move(name); }

 private:  // Testing purpose.
  std::string op_name_;
  std::vector<std::pair<std::string, std::string>> tags_;
  std::vector<std::pair<std::string, std::string>> logs_;
};

class DummyProvider : public TracingOpsProvider {
 public:
  explicit DummyProvider(std::string service)
      : service_name_(std::move(service)) {}

  otel::nostd::shared_ptr<otel::trace::Span> StartSpanWithOptions(
      otel::nostd::string_view operation_name,
      const SpanStartOptions& options) const noexcept override {
    otel::nostd::shared_ptr<otel::trace::Span> span(new DummySpan());
    auto* dummy = static_cast<DummySpan*>(span.get());
    dummy->set_op_name(
        std::string(operation_name.data(), operation_name.size()));
    // Record the span kind as a tag for the test to observe.
    dummy->SetAttribute(
        kSpanKind,
        options.kind == otel::trace::SpanKind::kClient ? "client" : "server");
    for (auto&& [k, v] : options.attributes) {
      dummy->SetAttribute(k, v);
    }
    return span;
  }

  void SetFrameworkTag(otel::trace::Span* span, otel::nostd::string_view key,
                       const otel::common::AttributeValue& value) override {
    if (ToStd(key) == ext::kInvocationStatus) {
      span->SetAttribute("dummy.invocation-status", value);
    } else {
      FLARE_CHECK(0);
    }
  }

  bool Inject(const otel::trace::SpanContext&, std::string*) const override {
    return true;
  }

  std::optional<otel::trace::SpanContext> Extract(
      const std::string&) const override {
    return std::nullopt;
  }

  bool IsSampled(const otel::trace::Span&) const noexcept override {
    return true;
  }

 private:
  std::string service_name_;
};

constexpr auto kTagKey = "my fancy tag";
constexpr auto kTagValue = "and it's fancy value";
constexpr auto kLogValue = "boring value";

class TracedRelayService : public testing::SyncRelayService {
 public:
  explicit TracedRelayService(const Endpoint& ep) {
    FLARE_CHECK(channel_.Open("flare://" + ep.ToString()));
  }

  void Relay(const testing::RelayRequest& request,
             testing::RelayResponse* response,
             RpcServerController* controller) override {
    controller->SetTracingTag(kTagKey, kTagValue);
    controller->AddTracingLog(kLogValue);
    testing::EchoService_SyncStub stub(&channel_);

    RpcClientController ctlr;
    testing::EchoRequest req;
    req.set_body(request.body());
    if (auto result = stub.Echo(req, &ctlr)) {
      response->set_body(result->body());
    } else {
      controller->SetFailed(result.error().code(), result.error().message());
    }
  }

 private:
  RpcChannel channel_;
};

class TracedEchoService : public testing::SyncEchoService {
 public:
  void Echo(const testing::EchoRequest& request,
            testing::EchoResponse* response,
            RpcServerController* controller) override {
    controller->SetTracingTag(kTagKey, kTagValue);
    controller->AddTracingLog(kLogValue);
    response->set_body(request.body());
  }
};

class TracingIntegrationTest : public ::testing::Test {
 public:
  void SetUp() override {
    FLAGS_flare_rpc_start_new_trace_on_missing = true;
    FLAGS_flare_tracing_provider = "dummy";

    server_.AddProtocol("flare");
    server_.AddService(std::make_unique<TracedEchoService>());
    server_.AddService(std::make_unique<TracedRelayService>(listening_ep_));
    server_.ListenOn(listening_ep_);
    server_.Start();
  }

  void TearDown() override {
    server_.Stop();
    server_.Join();
  }

 protected:
  Endpoint listening_ep_ = testing::PickAvailableEndpoint();

 private:
  Server server_;
};

TEST_F(TracingIntegrationTest, All) {
  RpcChannel channel;
  FLARE_CHECK(channel.Open("flare://" + listening_ep_.ToString()));

  RpcClientController ctlr;
  testing::RelayRequest req;
  req.set_body("hello");
  testing::RelayService_SyncStub stub(&channel);
  ASSERT_EQ("hello", stub.Relay(req, &ctlr)->body());

  // Wait until spans are reported.
  this_fiber::SleepFor(2s);  // Far more than enough.
  ASSERT_EQ(3, reported_spans.size());

  ASSERT_THAT(std::vector({reported_spans[0].method, reported_spans[1].method,
                           reported_spans[2].method}),
              ::testing::UnorderedElementsAre(
                  // Server-side in echo-server.
                  "flare.testing.EchoService.Echo",
                  // Client-side in relay-server.
                  "flare.testing.EchoService.Echo",
                  // Server-side in relay-server.
                  "flare.testing.RelayService.Relay"));

  for (auto&& span : reported_spans) {  // Server spans.
    std::unordered_map tag{span.tags.begin(), span.tags.end()};
    if (tag[kSpanKind] == "client") {
      continue;
    }
    ASSERT_THAT(span.tags, ::testing::UnorderedElementsAre(
                               std::pair(kSpanKind, "server"),
                               std::pair("dummy.invocation-status", "0"),
                               std::pair(kTagKey, kTagValue)));
    // An empty log key (single-arg `AddTracingLog`) materializes as the default
    // event name "log" (@sa: `QuickerSpan::FlushBufferedOps`).
    ASSERT_THAT(span.logs, ::testing::ElementsAre(std::pair("log", kLogValue)));
  }
}

namespace {

std::unique_ptr<DummyProvider> NewProvider(
    const TracingOpsProviderOptions& options) {
  return std::make_unique<DummyProvider>(options.service);
}

}  // namespace

FLARE_TRACING_REGISTER_TRACER_OPS_PROVIDER_FACTORY("dummy", NewProvider);

}  // namespace flare::tracing

FLARE_TEST_MAIN
