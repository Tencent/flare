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

#ifndef FLARE_RPC_BINLOG_LOG_READER_H_
#define FLARE_RPC_BINLOG_LOG_READER_H_

#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "flare/base/experimental/uuid.h"
#include "flare/rpc/binlog/dry_runner.h"

namespace flare::binlog {

// This class helps you to read  an incoming call when performing dry run.
class IncomingCallReader {
 public:
  using Tags = std::unordered_map<std::string, std::string>;

  bool InitializeWithProvider(DryRunIncomingCall* provider);

  // Accessors to fields parsed by `Prepare()`.
  const experimental::Uuid& GetHandlerUuid() const noexcept;

  // Passed through to the provider.
  const std::string& GetCorrelationId() const noexcept;
  const Tags& GetUserTags() const noexcept;
  const Tags& GetSystemTags() const noexcept;
  const std::string& GetSystemContext() const noexcept;
  const std::vector<DryRunPacket>& GetIncomingPackets() const noexcept;
  void CaptureOutgoingPacket(const PacketDesc& packet);

 private:
  DryRunIncomingCall* provider_{};

  // These fields are parsed by `Prepare()`.
  experimental::Uuid handler_uuid_;
};

// Reads from an outgoing call.
class OutgoingCallReader {
 public:
  using Tags = std::unordered_map<std::string, std::string>;

  bool InitializeWithProvider(DryRunOutgoingCall* provider);

  // Passed through to the provider.
  const std::string& GetCorrelationId() const noexcept;
  const Tags& GetUserTags() const noexcept;
  const Tags& GetSystemTags() const noexcept;
  const std::string& GetSystemContext() const noexcept;
  void CaptureOutgoingPacket(const PacketDesc& packet);

  // Set the timestamp this call is started in dry-run. Other method can use
  // this timestamp to emulate delays when necessary.
  void SetStartTimestamp(const std::chrono::steady_clock::time_point& ts);

  // Get an incoming packet. This method emulate the packet delay for you.
  Future<Expected<DryRunPacket, Status>> TryGetIncomingPacketEnumlatingDelay(
      std::size_t index);

 private:
  DryRunOutgoingCall* provider_{};
  std::chrono::steady_clock::time_point start_ts_;
};

// Reads a dry-run context.
class LogReader {
 public:
  bool InitializeWithProvider(std::unique_ptr<DryRunContext> provider);

  // Get reader for the incoming call.
  IncomingCallReader* GetIncomingCall();

  // Get reader for the outgoing call.
  Expected<OutgoingCallReader*, Status> TryStartOutgoingCall(
      const std::string& correlation_id);

  // Passed through to the provider.
  void SetInvocationStatus(std::string status);

  void WriteReport(NoncontiguousBuffer* buffer) const;

 private:
  std::unique_ptr<DryRunContext> provider_;

  IncomingCallReader incoming_;
  std::mutex lock_;
  std::list<OutgoingCallReader> outgoings_;
};

}  // namespace flare::binlog

#endif  // FLARE_RPC_BINLOG_LOG_READER_H_
