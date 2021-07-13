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

#include "flare/rpc/binlog/testing.h"

#include <any>
#include <string>
#include <utility>
#include <vector>

#include "flare/rpc/binlog/dumper.h"

namespace flare::binlog {

TestingPacketDesc::TestingPacketDesc(std::string s) : str(std::move(s)) {}

experimental::LazyEval<std::string> TestingPacketDesc::Describe() const {
  return str;
}

DumpingPacket NewIncomingPacket(DumpingCall* inspector, const PacketDesc& desc,
                                const std::string& sys_ctx) {
  experimental::LazyEval<std::string> prov_ctx;
  experimental::LazyEval<std::any> dump_ctx;
  inspector->CaptureIncomingPacket(desc, &dump_ctx, &prov_ctx);
  return DumpingPacket{
      .time_since_start = {},
      .provider_context = prov_ctx ? prov_ctx.Evaluate() : "",
      .system_context = sys_ctx,
      .dumper_context = dump_ctx ? dump_ctx.Evaluate() : std::any()};
}

DumpingPacket NewOutgoingPacket(DumpingCall* inspector, const PacketDesc& desc,
                                const std::string& sys_ctx) {
  experimental::LazyEval<std::string> prov_ctx;
  experimental::LazyEval<std::any> dump_ctx;
  inspector->CaptureOutgoingPacket(desc, &dump_ctx, &prov_ctx);
  return DumpingPacket{
      .time_since_start = {},
      .provider_context = prov_ctx ? prov_ctx.Evaluate() : "",
      .system_context = sys_ctx,
      .dumper_context = dump_ctx ? dump_ctx.Evaluate() : std::any()};
}

void NullDumpingCall::CaptureIncomingPacket(
    const PacketDesc& packet, experimental::LazyEval<std::any>* dumper_ctx,
    experimental::LazyEval<std::string>* prov_ctx) {}

void NullDumpingCall::CaptureOutgoingPacket(
    const PacketDesc& packet, experimental::LazyEval<std::any>* dumper_ctx,
    experimental::LazyEval<std::string>* prov_ctx) {}

void NullDumpingCall::SetCorrelationId(std::string cid) {}

void NullDumpingCall::SetTimestamps(
    std::chrono::steady_clock::time_point start_ts,
    std::chrono::steady_clock::time_point finish_ts) {}

void NullDumpingCall::SetSystemTags(Tags tags) {}

void NullDumpingCall::SetUserTags(Tags tags) {}

void NullDumpingCall::SetLogs(std::vector<std::string> logs) {}

void NullDumpingCall::SetSystemContext(std::string ctx) {}

void NullDumpingCall::SetIncomingPackets(std::vector<DumpingPacket> pkts) {}

void NullDumpingCall::SetOutgoingPackets(std::vector<DumpingPacket> pkts) {}

void IdentityDumpingCall::SetCorrelationId(std::string cid) {
  correlation_id_ = cid;
}

const std::string& IdentityDumpingCall::GetCorrelationId() const noexcept {
  return correlation_id_;
}

void IdentityDumpingCall::SetTimestamps(
    std::chrono::steady_clock::time_point start_ts,
    std::chrono::steady_clock::time_point finish_ts) {
  start_ts_ = start_ts;
  finish_ts_ = finish_ts;
}

std::pair<std::chrono::steady_clock::time_point,
          std::chrono::steady_clock::time_point>
IdentityDumpingCall::GetTimestamps() const noexcept {
  return std::pair(start_ts_, finish_ts_);
}

void IdentityDumpingCall::SetSystemTags(Tags tags) {
  sys_tags_ = std::move(tags);
}

const IdentityDumpingCall::Tags& IdentityDumpingCall::GetSystemTags()
    const noexcept {
  return sys_tags_;
}

void IdentityDumpingCall::SetUserTags(Tags tags) {
  user_tags_ = std::move(tags);
}

const IdentityDumpingCall::Tags& IdentityDumpingCall::GetUserTags()
    const noexcept {
  return user_tags_;
}

void IdentityDumpingCall::SetLogs(std::vector<std::string> logs) {
  logs_ = std::move(logs);
}

const std::vector<std::string>& IdentityDumpingCall::GetLogs() const noexcept {
  return logs_;
}

void IdentityDumpingCall::SetSystemContext(std::string ctx) { sys_ctx_ = ctx; }

const std::string& IdentityDumpingCall::GetSystemContext() const noexcept {
  return sys_ctx_;
}

void IdentityDumpingCall::SetIncomingPackets(std::vector<DumpingPacket> pkts) {
  incoming_pkts_ = std::move(pkts);
}

const std::vector<DumpingPacket>& IdentityDumpingCall::GetIncomingPackets()
    const noexcept {
  return incoming_pkts_;
}

void IdentityDumpingCall::SetOutgoingPackets(std::vector<DumpingPacket> pkts) {
  outgoing_pkts_ = std::move(pkts);
}

const std::vector<DumpingPacket>& IdentityDumpingCall::GetOutgoingPackets()
    const noexcept {
  return outgoing_pkts_;
}

}  // namespace flare::binlog
