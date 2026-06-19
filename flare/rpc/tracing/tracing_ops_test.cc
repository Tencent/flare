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

#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

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

#include "flare/rpc/tracing/framework_tags.h"
#include "flare/rpc/tracing/string_view_interop.h"
#include "flare/testing/main.h"

using namespace std::literals;

namespace flare::tracing {

namespace {

// Stringify an `AttributeValue` for assertion purposes.
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
          return {};  // `span<>` alternatives, not used by these tests.
        }
      },
      v);
}

}  // namespace

// A recording fake span. (OpenTelemetry's bundled noop span discards
// everything, so it can't be asserted on.)
class DummySpan : public otel::trace::Span {
 public:
  void SetAttribute(otel::nostd::string_view key,
                    const otel::common::AttributeValue& value) noexcept
      override {
    tags.push_back({std::string(key.data(), key.size()), AttrToString(value)});
  }

  void AddEvent(otel::nostd::string_view) noexcept override {}
  void AddEvent(otel::nostd::string_view,
                otel::common::SystemTimestamp) noexcept override {}
  void AddEvent(otel::nostd::string_view name, otel::common::SystemTimestamp,
                const otel::common::KeyValueIterable& attributes) noexcept
      override {
    attributes.ForEachKeyValue(
        [&](otel::nostd::string_view,
            otel::common::AttributeValue value) noexcept {
          logs.push_back(
              {std::string(name.data(), name.size()), AttrToString(value)});
          return true;
        });
  }

  void SetStatus(otel::trace::StatusCode,
                 otel::nostd::string_view) noexcept override {}
  void UpdateName(otel::nostd::string_view name) noexcept override {
    op_name = std::string(name.data(), name.size());
  }
  void End(const otel::trace::EndSpanOptions&) noexcept override {}
  otel::trace::SpanContext GetContext() const noexcept override {
    return otel::trace::SpanContext::GetInvalid();
  }
  bool IsRecording() const noexcept override { return true; }

 public:  // Testing purpose.
  std::string op_name;
  inline static std::vector<std::pair<std::string, std::string>> tags;
  inline static std::vector<std::pair<std::string, std::string>> logs;
};

class DummyProvider : public TracingOpsProvider {
 public:
  otel::nostd::shared_ptr<otel::trace::Span> StartSpanWithOptions(
      otel::nostd::string_view operation_name,
      const SpanStartOptions&) const noexcept override {
    otel::nostd::shared_ptr<otel::trace::Span> span(new DummySpan());
    static_cast<DummySpan*>(span.get())->op_name =
        std::string(operation_name.data(), operation_name.size());
    return span;
  }

  void SetFrameworkTag(otel::trace::Span* span, otel::nostd::string_view key,
                       const otel::common::AttributeValue& value) override {
    if (ToStd(key) == ext::kTrackingId) {
      span->SetAttribute("dummy.tracking-id", value);
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
};

TEST(TracingOps, Noop) {
  TracingOps ops(nullptr);
  auto span = ops.StartSpanWithLazyOptions("my op", [](auto&&) {});
  ASSERT_FALSE(span.span_);
  span.SetStandardTag("peer.ipv4", "127.0.0.1"s);
  span.SetFrameworkTag(ToOtel(ext::kTrackingId), "tracking-id"s);
  span.SetUserTag("user-tag", "value"s);
  span.Report();
  // Nothing should happen.
}

TEST(TracingOps, DummyProvider) {
  TracingOps ops(std::make_unique<DummyProvider>());
  auto span = ops.StartSpanWithLazyOptions("my op", [](auto&&) {});
  span.SetStandardTag("peer.ipv4", "127.0.0.1"s);
  span.SetFrameworkTag(ToOtel(ext::kTrackingId), "tracking-id"s);
  span.SetUserTag("user-tag", "value"s);
  auto p = dynamic_cast<DummySpan*>(span.span_.get());
  ASSERT_EQ("my op", p->op_name);
  span.Report();
  ASSERT_THAT(DummySpan::tags,
              ::testing::ElementsAre(
                  std::pair("peer.ipv4", "127.0.0.1"),
                  std::pair("dummy.tracking-id", "tracking-id"),  // Translated.
                  std::pair("user-tag", "value")));

  // Let the report actually happen (so as to flush DPC queue), otherwise we'd
  // have a hard time in draining DPC queue after leaving `main`.
  std::this_thread::sleep_for(1s);
}

}  // namespace flare::tracing

FLARE_TEST_MAIN
