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

#ifndef FLARE_RPC_BINLOG_UTIL_PROTO_DUMPER_H_
#define FLARE_RPC_BINLOG_UTIL_PROTO_DUMPER_H_

#include <chrono>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "flare/rpc/binlog/dumper.h"
#include "flare/rpc/binlog/util/proto_binlog.pb.h"

namespace flare::binlog {

// This class helps you implementing `DumpingCall` in terms of `proto::Call`.
//
// NOT INTENDED FOR PUBLIC USE.
class ProtoDumpingCall : public DumpingCall {
 public:
  void SetCorrelationId(std::string cid) override {
    call_.set_correlation_id(cid);
  }
  const std::string& GetCorrelationId() const noexcept {
    return call_.correlation_id();
  }
  void SetTimestamps(std::chrono::steady_clock::time_point start_ts,
                     std::chrono::steady_clock::time_point finish_ts) override {
    start_ts_ = start_ts;
    finish_ts_ = finish_ts;
  }
  std::pair<std::chrono::steady_clock::time_point,
            std::chrono::steady_clock::time_point>
  GetTimestamps() const noexcept {
    return std::pair(start_ts_, finish_ts_);
  }
  void SetSystemTags(Tags tags) override {
    call_.mutable_system_tags()->insert(tags.begin(), tags.end());
    sys_tags_ = std::move(tags);
  }
  const Tags& GetSystemTags() const noexcept { return sys_tags_; }
  void SetUserTags(Tags tags) override {
    call_.mutable_user_tags()->insert(tags.begin(), tags.end());
    user_tags_ = std::move(tags);
  }
  const Tags& GetUserTags() const noexcept { return user_tags_; }
  void SetLogs(std::vector<std::string> logs) override {
    for (auto&& e : logs) {
      call_.add_logs(e);
    }
    logs_ = std::move(logs);
  }
  const std::vector<std::string>& GetLogs() const noexcept { return logs_; }
  void SetSystemContext(std::string ctx) override {
    call_.set_system_context(ctx);
  }
  const std::string& GetSystemContext() const noexcept {
    return call_.system_context();
  }
  void SetIncomingPackets(std::vector<DumpingPacket> pkts) override {
    for (auto&& e : pkts) {
      auto&& pkt = call_.add_incoming_pkts();
      pkt->set_time_since_start(e.time_since_start /
                                std::chrono::nanoseconds(1));
      pkt->set_provider_context(e.provider_context);
      pkt->set_system_context(e.system_context);
    }
    incoming_pkts_ = std::move(pkts);
  }
  const std::vector<DumpingPacket>& GetIncomingPackets() const noexcept {
    return incoming_pkts_;
  }
  void SetOutgoingPackets(std::vector<DumpingPacket> pkts) override {
    for (auto&& e : pkts) {
      auto&& pkt = call_.add_outgoing_pkts();
      pkt->set_time_since_start(e.time_since_start /
                                std::chrono::nanoseconds(1));
      pkt->set_provider_context(e.provider_context);
      pkt->set_system_context(e.system_context);
    }
    outgoing_pkts_ = std::move(pkts);
  }
  const std::vector<DumpingPacket>& GetOutgoingPackets() const noexcept {
    return outgoing_pkts_;
  }

  const proto::Call& GetMessage() const noexcept { return call_; }

 private:
  std::chrono::steady_clock::time_point start_ts_, finish_ts_;
  Tags sys_tags_, user_tags_;
  std::vector<std::string> logs_;
  std::vector<DumpingPacket> incoming_pkts_, outgoing_pkts_;

  proto::Call call_;
};

}  // namespace flare::binlog

#endif  // FLARE_RPC_BINLOG_UTIL_PROTO_DUMPER_H_
