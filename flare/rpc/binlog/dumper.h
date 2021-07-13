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

#ifndef FLARE_RPC_BINLOG_DUMPER_H_
#define FLARE_RPC_BINLOG_DUMPER_H_

#include <any>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "flare/base/buffer.h"
#include "flare/base/dependency_registry.h"
#include "flare/base/experimental/lazy_eval.h"
#include "flare/base/maybe_owning.h"
#include "flare/base/net/endpoint.h"
#include "flare/rpc/binlog/packet_desc.h"

// This file defines some classes used in dumping RPCs.
//
// RPCs are dumped via `CallDumper` to an implementation-defined storage. When
// needed, the user can use what's dumped by the implementation to do a "dry
// run" (for testing purpose, for example).
//
// For detailed explanation, @sa: `doc/rpc-log-and-dry-run.md`.

// Design considerations:
//
// Indeed we can use a design similar to OpenTracing here. However, design of
// OpenTracing requires the implementation to merge spans afterwards. That
// decision makes perfect sense for distributed tracing due to its distributed
// nature (and hence the need to (re-)merge anyway), but unnecessarily
// complicates things for dumping RPCs.

// We're using system timestamp in binlog classes. As binlog is likely to be
// used on a machine different from where it was captured, steady clock hardly
// makes sense.

namespace flare::binlog {

// Represents a packet being dumped.
struct DumpingPacket {
  std::chrono::nanoseconds time_since_start;

  // Filled by binlog provider. Kept persistent by the dumper.
  //
  // This context can be used for passing data between dumper and dry-runner.
  std::string provider_context;

  // Filled by the framework. Kept persistent by the dumper.
  //
  // Later the framework will need it for performing dry-run.
  std::string system_context;

  // Not kept by the dumper during serialization.
  //
  // This field is provided for passing context out from `CaptureXxxPacket` to
  // other methods of dumper.
  std::any dumper_context;
};

// Represents a call, either an incoming (we're acting as server) or an outgoing
// (we're acting as a client.) one.
class DumpingCall {
 public:
  using Tags = std::unordered_map<std::string, std::string>;

  virtual ~DumpingCall() = default;

  // The implementation can capture whatever it interested in the packet being
  // sent / received.
  //
  // I'm not sure if we should separate it into `CaptureIncomingPacket` and
  // `CaptureOutgoingPacket`, that seems to be too complicated.
  //
  // Note that THIS METHOD METHOD IS CALLED IN CRITICAL PATH, so be quick.
  //
  // `dumper_ctx`: Dumper context (only accessible during this run).
  // `prov_ctx`: Dry-runner context (kept persistent and accessible to
  //             dry-runner.).
  virtual void CaptureIncomingPacket(
      const PacketDesc& packet, experimental::LazyEval<std::any>* dumper_ctx,
      experimental::LazyEval<std::string>* prov_ctx) = 0;
  virtual void CaptureOutgoingPacket(
      const PacketDesc& packet, experimental::LazyEval<std::any>* dumper_ctx,
      experimental::LazyEval<std::string>* prov_ctx) = 0;

  // Uniquely identifies a call
  virtual void SetCorrelationId(std::string cid) = 0;

  // Timestamps.
  virtual void SetTimestamps(
      std::chrono::steady_clock::time_point start_ts,
      std::chrono::steady_clock::time_point finish_ts) = 0;

  // System tags. @sa: `tags.h`
  //
  // Note that tag keys can be added in the future without further notice.
  // Therefore, to keep forward compatibility, you should always serialize *all*
  // tags, even those not recognized by your implementation.
  virtual void SetSystemTags(Tags tags) = 0;

  // User tags.
  virtual void SetUserTags(Tags tags) = 0;

  // For debugging purpose only. The implementation can safely ignore it.
  virtual void SetLogs(std::vector<std::string> logs) = 0;

  // Saves whatever interesting to the framework, it's opaque to binlog
  // provider.
  virtual void SetSystemContext(std::string ctx) = 0;

  // Packets captured previously by `CaptureXxxPacket`, along with some
  // framework stuff.
  virtual void SetIncomingPackets(std::vector<DumpingPacket> pkts) = 0;
  virtual void SetOutgoingPackets(std::vector<DumpingPacket> pkts) = 0;
};

// Represents an entire RPC log being dumped.
class DumpingLog {
 public:
  virtual ~DumpingLog() = default;

  // Returns the object represents this RPC.
  virtual DumpingCall* GetIncomingCall() = 0;

  // Returns a new call instance to represent a new call to the outside. If the
  // implementation does not care about outgoing calls, it can return `nullptr`
  // instead.
  //
  // Ownership of the pointer returned is kept by this class.
  virtual DumpingCall* StartOutgoingCall() = 0;

  // Dumps the log. Called out of the critical path, being slow does not matter
  // much.
  virtual void Dump() = 0;

  // Called if this log was aborted. You can leave this method as-is if nothing
  // special needs to be done.
  virtual void Abort() {}
};

// Implementation of this interface is responsible for dumping sessions logged
// by the framework. It's possible that what's dumped by the implementation is
// replayed by the user for (e.g.) testing purpose.
//
// The implementation must be thread-safe.
class Dumper {
 public:
  virtual ~Dumper() = default;

  // Start a new call.
  virtual std::unique_ptr<DumpingLog> StartDumping(/* Something? */) = 0;
};

// Tests if we need to sample the incoming RPC.
//
// I'm not sure if we should provide a `Sampled()` method in `Dumper` instead,
// that's what OpenTracing have done (but again, given its distributed nature
// ...).
bool AcquireSamplingQuotaForDumping();

// Generate a correlation ID.
std::string NewCorrelationId();

// Get binlog dumper enabled by the user.
Dumper* GetDumper();

FLARE_DECLARE_CLASS_DEPENDENCY_REGISTRY(dumper_registry, Dumper);

}  // namespace flare::binlog

#define FLARE_RPC_BINLOG_REGISTER_DUMPER(Name, Factory)                   \
  FLARE_REGISTER_CLASS_DEPENDENCY_FACTORY(flare::binlog::dumper_registry, \
                                          Name, Factory)

#endif  // FLARE_RPC_BINLOG_DUMPER_H_
