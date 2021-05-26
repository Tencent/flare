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

#include "flare/rpc/internal/session_context.h"

#include <limits>

#include "flare/base/object_pool.h"

namespace flare {

template <>
struct PoolTraits<rpc::SessionContext> {
  static constexpr auto kType = PoolType::MemoryNodeShared;
  static constexpr auto kLowWaterMark = 16384;
  static constexpr auto kHighWaterMark =
      std::numeric_limits<std::size_t>::max();
  static constexpr auto kMaxIdle = std::chrono::seconds(10);
  static constexpr auto kMinimumThreadCacheSize = 4096;
  static constexpr auto kTransferBatchSize = 1024;

  static void OnPut(rpc::SessionContext* ctx) {
    ctx->binlog.correlation_id.clear();
    ctx->binlog.dumper = std::nullopt;
    ctx->binlog.dry_runner.reset();

    FLARE_CHECK(!ctx->tracing.server_span.Tracing());
  }
};

}  // namespace flare

namespace flare::rpc {

fiber::ExecutionLocal<SessionContext> session_context;

void InitializeSessionContext() {
  session_context.UnsafeInit(
      object_pool::Get<SessionContext>().Leak(),
      [](auto* p) { object_pool::Put<SessionContext>(p); });
}

bool IsBinlogDumpContextPresent() {
  return fiber::ExecutionContext::Current() &&
         rpc::session_context->binlog.dumper;
}

bool IsDryRunContextPresent() {
  // Note that whether or not dry runner is present (i.e., we're in dry-run
  // environment) is process-wide, and we don't have to test for its presence
  // each time we get called. Therefore a static constant should work.
  static const bool kIsDryRun = binlog::GetDryRunner();
  return kIsDryRun;
}

bool IsTracedContextPresent() {
  return fiber::ExecutionContext::Current() &&
         rpc::session_context->tracing.server_span.Tracing();
}

RefPtr<fiber::ExecutionContext> CaptureSessionContext() {
  return RefPtr(ref_ptr, fiber::ExecutionContext::Current());
}

}  // namespace flare::rpc
