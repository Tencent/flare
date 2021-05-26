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

#ifndef FLARE_RPC_BINLOG_DRY_RUNNER_H_
#define FLARE_RPC_BINLOG_DRY_RUNNER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "flare/base/buffer.h"
#include "flare/base/dependency_registry.h"
#include "flare/base/expected.h"
#include "flare/base/future.h"
#include "flare/base/net/endpoint.h"
#include "flare/base/status.h"
#include "flare/fiber/timer.h"
#include "flare/rpc/binlog/packet_desc.h"

// Here we defines several classes essential to doing dry-run.
//
// There's no much performance requirement here as we don't expect dry-run to be
// very performant anyway..

namespace flare::binlog {

// Not declared as enum class intentionally.
enum DryRunStatus {
  STATUS_EOF = 1,
  STATUS_NOT_FOUND = 2,
  STATUS_INTERNAL_ERROR = 3,
};

// Represents a packet captured during RPC dump and is available for dry run.
struct DryRunPacket {
  std::chrono::nanoseconds time_since_start;
  std::string provider_ctx;
  std::string system_ctx;
};

// Represents an incoming call.
class DryRunIncomingCall {
 public:
  using Tags = std::unordered_map<std::string, std::string>;

  virtual ~DryRunIncomingCall() = default;

  // Accessors to data provided by the implementation.
  virtual const std::string& GetCorrelationId() const noexcept = 0;
  virtual const Tags& GetSystemTags() const noexcept = 0;
  virtual const Tags& GetUserTags() const noexcept = 0;
  virtual const std::string& GetSystemContext() const noexcept = 0;

  // Get all incoming packets.
  virtual const std::vector<DryRunPacket>& GetIncomingPackets()
      const noexcept = 0;

  // The framework will call this method whenever a packet is (scheduled to be)
  // sent out to the request generator. The implementation is free to capture
  // whatever it wants for later inspection.
  //
  // Performance does not matter much, as dry-run is not performance-wise
  // critical anyway.
  //
  // If you're going to report the packet to outside (via network, e.g.), it's
  // suggested that you do it in a non-blocking fashion.
  virtual void CaptureOutgoingPacket(const PacketDesc& packet) = 0;
};

// Represents an outgoing call.
class DryRunOutgoingCall {
 public:
  using Tags = std::unordered_map<std::string, std::string>;

  virtual ~DryRunOutgoingCall() = default;

  // Accessors to data provided by the implementation.
  virtual const std::string& GetCorrelationId() const noexcept = 0;
  virtual const Tags& GetSystemTags() const noexcept = 0;
  virtual const Tags& GetUserTags() const noexcept = 0;
  virtual const std::string& GetSystemContext() const noexcept = 0;

  // Try read an incoming packet.
  virtual Future<Expected<DryRunPacket, Status>> TryGetIncomingPacket(
      std::size_t index) = 0;

  // The framework will call this method whenever a packet is (scheduled to be)
  // sent out to the (now mocked) backend server.
  //
  // If you're going to report the packet to outside (via network, e.g.), it's
  // suggested that you do it in a non-blocking fashion.
  virtual void CaptureOutgoingPacket(const PacketDesc& packet) = 0;
};

// This class is responsible for a single RPC in dry-run mode.
class DryRunContext {
 public:
  virtual ~DryRunContext() = default;

  // It is expected that request generator will send the incoming call that was
  // captured by `Dumper` so that it's available to us now. This method returns
  // that information.
  virtual DryRunIncomingCall* GetIncomingCall() = 0;

  // Try find an outgoing call.
  virtual Expected<DryRunOutgoingCall*, Status> TryGetOutgoingCall(
      const std::string& correlation_id) = 0;

  // Called upon RPC completion to notify the implementation about the (dry-run)
  // invocation result.
  virtual void SetInvocationStatus(std::string status) = 0;

  // Serialize the dry run result to byte stream, which is later sent back to
  // request generator.
  //
  // The implementation may also report the result via a side channel (e.g. by
  // calling third-party in the implementation.).
  virtual void WriteReport(NoncontiguousBuffer* buffer) const = 0;
};

// When doing a dry run, the framework uses this class (and its production) to
// help it to:
//
// - (Server side) Parse request received from request generator.
// - (Server side) Pack response into a format recognized by request generator.
// - (Client side) Find serialized data for "mocking" the response.
class DryRunner {
 public:
  virtual ~DryRunner() = default;

  enum ByteStreamParseStatus { Success, NeedMore, Error };

  // Try to extract a `DryRunContext` from byte stream.
  virtual ByteStreamParseStatus ParseByteStream(
      NoncontiguousBuffer* buffer, std::unique_ptr<DryRunContext>* context) = 0;
};

// Get binlog dumper enabled by the user.
DryRunner* GetDryRunner();

FLARE_DECLARE_CLASS_DEPENDENCY_REGISTRY(dry_runner_registry, DryRunner);

}  // namespace flare::binlog

#define FLARE_RPC_BINLOG_REGISTER_DRY_RUNNER(Name, Factory)                   \
  FLARE_REGISTER_CLASS_DEPENDENCY_FACTORY(flare::binlog::dry_runner_registry, \
                                          Name, Factory)

#endif  // FLARE_RPC_BINLOG_DRY_RUNNER_H_
