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

#ifndef FLARE_RPC_BINLOG_TESTING_H_
#define FLARE_RPC_BINLOG_TESTING_H_

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "flare/base/down_cast.h"
#include "flare/rpc/binlog/dumper.h"
#include "flare/rpc/binlog/packet_desc.h"
#include "flare/rpc/protocol/message.h"

// Some testing utilities go here.
//
// Well, they're not put into `flare/testing/` as they're not intended for
// public use.

namespace flare::binlog {

// FOR TESTING PURPOSE ONLY. Represents a fake "string packet".
struct TestingPacketDesc : TypedPacketDesc<TestingPacketDesc> {
  std::string str;

  explicit TestingPacketDesc(std::string s);
  experimental::LazyEval<std::string> Describe() const override;
};

// Create a `DumpingPacket`.
DumpingPacket NewIncomingPacket(DumpingCall* inspector, const PacketDesc& desc,
                                const std::string& sys_ctx = "");
DumpingPacket NewOutgoingPacket(DumpingCall* inspector, const PacketDesc& desc,
                                const std::string& sys_ctx = "");

// This implementation ignores everything.
class NullDumpingCall : public DumpingCall {
 public:
  void CaptureIncomingPacket(
      const PacketDesc& packet, experimental::LazyEval<std::any>* dumper_ctx,
      experimental::LazyEval<std::string>* prov_ctx) override;
  void CaptureOutgoingPacket(
      const PacketDesc& packet, experimental::LazyEval<std::any>* dumper_ctx,
      experimental::LazyEval<std::string>* prov_ctx) override;
  void SetCorrelationId(std::string cid) override;
  void SetTimestamps(
      std::chrono::steady_clock::time_point start_ts,
      std::chrono::steady_clock::time_point finish_ts) override;
  void SetSystemTags(Tags tags) override;
  void SetUserTags(Tags tags) override;
  void SetLogs(std::vector<std::string> logs) override;
  void SetSystemContext(std::string ctx) override;
  void SetIncomingPackets(std::vector<DumpingPacket> pkts) override;
  void SetOutgoingPackets(std::vector<DumpingPacket> pkts) override;
};

// This implementation saves what's given for later inspection. You still need
// to implement `CaptureXxxPacket` yourself.
class IdentityDumpingCall : public DumpingCall {
 public:
  void SetCorrelationId(std::string cid) override;
  const std::string& GetCorrelationId() const noexcept;
  void SetTimestamps(
      std::chrono::steady_clock::time_point start_ts,
      std::chrono::steady_clock::time_point finish_ts) override;
  std::pair<std::chrono::steady_clock::time_point,
            std::chrono::steady_clock::time_point>
  GetTimestamps() const noexcept;
  void SetSystemTags(Tags tags) override;
  const Tags& GetSystemTags() const noexcept;
  void SetUserTags(Tags tags) override;
  const Tags& GetUserTags() const noexcept;
  void SetLogs(std::vector<std::string> logs) override;
  const std::vector<std::string>& GetLogs() const noexcept;
  void SetSystemContext(std::string ctx) override;
  const std::string& GetSystemContext() const noexcept;
  void SetIncomingPackets(std::vector<DumpingPacket> pkts) override;
  const std::vector<DumpingPacket>& GetIncomingPackets() const noexcept;
  void SetOutgoingPackets(std::vector<DumpingPacket> pkts) override;
  const std::vector<DumpingPacket>& GetOutgoingPackets() const noexcept;

 private:
  std::string correlation_id_;
  std::chrono::steady_clock::time_point start_ts_, finish_ts_;
  Tags sys_tags_, user_tags_;
  std::vector<std::string> logs_;
  std::string sys_ctx_;
  std::vector<DumpingPacket> incoming_pkts_, outgoing_pkts_;
};

}  // namespace flare::binlog

#endif  // FLARE_RPC_BINLOG_TESTING_H_
