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

#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "opentracing/ext/tags.h"
#include "gflags/gflags.h"
#include "gmock/gmock-matchers.h"
#include "gtest/gtest.h"

#include "flare/base/overloaded.h"
#include "flare/fiber/this_fiber.h"
#include "flare/rpc/rpc_channel.h"
#include "flare/rpc/rpc_client_controller.h"
#include "flare/rpc/rpc_server_controller.h"
#include "flare/rpc/server.h"
#include "flare/rpc/tracing/framework_tags.h"
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

std::string ToString(const opentracing::Value& v) {
  std::stringstream ss;
  opentracing::Value::visit(
      v, Overloaded{
             [&](std::nullptr_t) {},
             [&](const auto& v) -> std::void_t<decltype(ss << v)> { ss << v; },
             [&](auto&&) {}});
  return ss.str();
}

opentracing::SpanContext* null_span_context = nullptr;
opentracing::Tracer* null_tracer = nullptr;

}  // namespace

struct MaterializedSpan {
  std::string method;
  std::vector<std::pair<std::string, std::string>> tags, logs;
};

std::mutex lock;
std::vector<MaterializedSpan> reported_spans;

class DummySpan : public opentracing::Span {
 public:
  void FinishWithOptions(
      const opentracing::FinishSpanOptions&) noexcept override {
    MaterializedSpan ms = {.method = op_name_, .tags = tags_, .logs = logs_};
    std::scoped_lock _(lock);
    reported_spans.push_back(ms);
  }

  void SetOperationName(opentracing::string_view name) noexcept override {
    op_name_ = name;
  }

  void SetTag(opentracing::string_view key,
              const opentracing::Value& value) noexcept override {
    tags_.push_back(std::pair(key, ToString(value)));
  }

  // Not used.
  void SetBaggageItem(opentracing::string_view,
                      opentracing::string_view) noexcept override {}
  std::string BaggageItem(opentracing::string_view) const noexcept override {
    return "";
  }

  void Log(std::initializer_list<
           std::pair<opentracing::string_view, opentracing::Value>>
               vs) noexcept override {
    for (auto&& [k, v] : vs) {
      logs_.push_back(std::pair(k, ToString(v)));
    }
  }

  const opentracing::SpanContext& context() const noexcept override {
    return *null_span_context;  // U.B.?
  }

  const opentracing::Tracer& tracer() const noexcept override {
    return *null_tracer;  // U.B.?
  }

 private:  // Testing purpose.
  std::string op_name_;
  std::vector<std::pair<std::string, std::string>> tags_;
  std::vector<std::pair<std::string, std::string>> logs_;
};

class DummyProvider : public TracingOpsProvider {
 public:
  explicit DummyProvider(std::string service)
      : service_name_(std::move(service)) {}

  std::unique_ptr<opentracing::Span> StartSpanWithOptions(
      opentracing::string_view operation_name,
      const opentracing::StartSpanOptions& options) const noexcept override {
    auto result = std::make_unique<DummySpan>();
    result->SetOperationName(std::string(operation_name));
    for (auto&& [k, v] : options.tags) {
      result->SetTag(k, v);
    }
    return result;
  }

  void SetFrameworkTag(opentracing::Span* span, opentracing::string_view key,
                       const opentracing::Value& value) override {
    if (key == ext::kInvocationStatus) {
      span->SetTag("dummy.invocation-status",
                   std::to_string(value.get<std::int64_t>()));
    } else {
      FLARE_CHECK(0);
    }
  }

  bool Inject(const opentracing::SpanContext& sc,
              std::string* out) const override {
    return true;
  }

  opentracing::expected<std::unique_ptr<opentracing::SpanContext>> Extract(
      const std::string& in) const override {
    return std::unique_ptr<opentracing::SpanContext>();
  }

  bool IsSampled(const opentracing::Span&) const noexcept override {
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
    if (tag[opentracing::ext::span_kind] ==
        opentracing::ext::span_kind_rpc_client) {
      continue;
    }
    ASSERT_THAT(span.tags,
                ::testing::UnorderedElementsAre(
                    std::pair(opentracing::ext::span_kind,
                              opentracing::ext::span_kind_rpc_server),
                    std::pair("dummy.invocation-status", "0"),
                    std::pair(kTagKey, kTagValue)));
    ASSERT_THAT(span.logs, ::testing::ElementsAre(std::pair("", kLogValue)));
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
