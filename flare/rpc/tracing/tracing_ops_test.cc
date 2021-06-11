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

#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "opentracing/ext/tags.h"
#include "googletest/gmock/gmock-matchers.h"
#include "googletest/gtest/gtest.h"

#include "flare/rpc/tracing/framework_tags.h"
#include "flare/rpc/tracing/string_view_interop.h"
#include "flare/testing/main.h"

using namespace std::literals;

namespace flare::tracing {

opentracing::SpanContext* null_span_context = nullptr;
opentracing::Tracer* null_tracer = nullptr;

class DummySpan : public opentracing::Span {
 public:
  void FinishWithOptions(
      const opentracing::FinishSpanOptions&) noexcept override {}

  void SetOperationName(opentracing::string_view name) noexcept override {
    op_name = name;
  }

  void SetTag(opentracing::string_view key,
              const opentracing::Value& value) noexcept override {
    tags.push_back(std::pair(key, value.get<std::string>()));
  }

  void SetBaggageItem(opentracing::string_view,
                      opentracing::string_view) noexcept override {}

  std::string BaggageItem(opentracing::string_view) const noexcept override {
    return "";
  }

  void Log(std::initializer_list<
           std::pair<opentracing::string_view, opentracing::Value>>) noexcept
      override {}

  const opentracing::SpanContext& context() const noexcept override {
    return *null_span_context;  // U.B.?
  }

  const opentracing::Tracer& tracer() const noexcept override {
    return *null_tracer;  // U.B.?
  }

 public:  // Testing purpose.
  std::string op_name;
  inline static std::vector<std::pair<std::string, std::string>> tags;
};

class DummyProvider : public TracingOpsProvider {
 public:
  std::unique_ptr<opentracing::Span> StartSpanWithOptions(
      opentracing::string_view operation_name,
      const opentracing::StartSpanOptions& options) const noexcept override {
    auto result = std::make_unique<DummySpan>();
    result->SetOperationName(operation_name);
    return result;
  }

  void SetFrameworkTag(opentracing::Span* span, opentracing::string_view key,
                       const opentracing::Value& value) override {
    if (key == ext::kTrackingId) {
      span->SetTag("dummy.tracking-id", value);
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
};

TEST(TracingOps, Noop) {
  TracingOps ops(nullptr);
  auto span = ops.StartSpanWithLazyOptions("my op", [](auto start_opts) {});
  ASSERT_EQ(nullptr, span.span_);
  span.SetStandardTag(opentracing::ext::peer_host_ipv4, "127.0.0.1"s);
  span.SetFrameworkTag(ext::kTrackingId, "tracking-id"s);
  span.SetUserTag("user-tag", "value"s);
  span.Report();
  // Nothing should happen.
}

TEST(TracingOps, DummyProvider) {
  TracingOps ops(std::make_unique<DummyProvider>());
  auto span = ops.StartSpanWithLazyOptions("my op", [](auto start_opts) {});
  span.SetStandardTag(opentracing::ext::peer_host_ipv4, "127.0.0.1"s);
  span.SetFrameworkTag(ext::kTrackingId, "tracking-id"s);
  span.SetUserTag("user-tag", "value"s);
  auto p = dynamic_cast<DummySpan*>(span.span_.get());
  ASSERT_EQ("my op", p->op_name);
  span.Report();
  ASSERT_THAT(DummySpan::tags,
              ::testing::ElementsAre(
                  std::pair(opentracing::ext::peer_host_ipv4, "127.0.0.1"),
                  std::pair("dummy.tracking-id", "tracking-id"),  // Translated.
                  std::pair("user-tag", "value")));

  // Let the report actually happen (so as to flush DPC queue), otherwise we'd
  // have a hard time in draining DPC queue after leaving `main`.
  std::this_thread::sleep_for(1s);
}

}  // namespace flare::tracing

FLARE_TEST_MAIN
