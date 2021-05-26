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

#ifndef FLARE_RPC_INTERNAL_SESSION_CONTEXT_H_
#define FLARE_RPC_INTERNAL_SESSION_CONTEXT_H_

#include <memory>
#include <optional>
#include <string>

#include "flare/base/ref_ptr.h"
#include "flare/fiber/execution_context.h"
#include "flare/rpc/binlog/log_reader.h"
#include "flare/rpc/binlog/log_writer.h"
#include "flare/rpc/tracing/tracing_ops.h"

namespace flare::rpc {

// Context about this RPC session.
//
// FOR INTERNAL USE ONLY. YOU MAY NOT TOUCH IT YOURSELF.
//
// TODO(luobogao): Consider move it into `flare/rpc/session_context.h`. It's
// used by several sub-systems in Flare.
struct SessionContext {
  // Tracing ID is used for ... tracking.
  //
  // It's propagated from the caller, and further propagated all the way down
  // this RPC by us.
  //
  // Beside, some component (e.g., LeFlow name resolver) might change their
  // behavior depending on this ID.
  std::string tracking_id;

  // Vector clock is used for determining a logic order between RPCs in this
  // RPC-chain.
  //
  // Some component (e.g., distributed logging system) may use this to reorder
  // data reported before displaying them to end user.
  //
  // TODO(luobogao): Vector clock.

  //////////////////////////////
  // Everything about binlog. //
  //////////////////////////////

  struct {
    // Correlation ID used for incoming RPC.
    std::string correlation_id;

    // At most one of `dumper` and `dry_runner` can be initialized (they can be
    // both left uninitialized if this RPC is not sampled for dumping.).

    // If set, it's the dumper responsible for this RPC.
    std::optional<binlog::LogWriter> dumper;

    // If set, it's the context object for performing dry run.
    std::unique_ptr<binlog::LogReader> dry_runner;
  } binlog;

  ///////////////////////////////
  // Everything about tracing. //
  ///////////////////////////////

  struct {
    // Pointer to distributed tracing provider we're currently using.
    tracing::TracingOps* tracing_ops;

    // `QuickerSpan` by itself is NOT thread-safe. You need to grab this lock
    // before touching it.
    //
    // Not making `QuickerSpan` itself thread-safe is primarily for perf.
    // reasons. Unless the user wants to add logs to `server_span`, the
    // framework always use it in single-threaded env., therefore there's no
    // point in locking inside it.
    std::mutex server_span_lock;

    // Span at server side. For client-side spans, it's`XxxClient` or
    // `XxxChannel`'s responsibility to keep it and is not store here.
    tracing::QuickerSpan server_span;
  } tracing;
};

extern fiber::ExecutionLocal<SessionContext> session_context;

// Initializes `session_context`. We do this explicitly for perf. reasons.
void InitializeSessionContext();

// Tests if current session is being dumped to binlog.
bool IsBinlogDumpContextPresent();

// Tests if we're running in dry-run context.
bool IsDryRunContextPresent();

// Tests if current session is being traced.
bool IsTracedContextPresent();

// Capture current session context (i.e., execution context), if we're indeed in
// one.
RefPtr<fiber::ExecutionContext> CaptureSessionContext();

}  // namespace flare::rpc

#endif  // FLARE_RPC_INTERNAL_SESSION_CONTEXT_H_
