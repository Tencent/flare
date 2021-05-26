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

#ifndef FLARE_RPC_BINLOG_UTIL_PROTO_DRY_RUNNER_H_
#define FLARE_RPC_BINLOG_UTIL_PROTO_DRY_RUNNER_H_

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "flare/rpc/binlog/dry_runner.h"
#include "flare/rpc/binlog/util/proto_binlog.pb.h"

namespace flare::binlog {

namespace detail {

// Implementation detail of `ProtoDryRunXxxCall`.
template <class T>
class ProtoDryRunCallImpl : public T {
  using Tags = std::unordered_map<std::string, std::string>;

 public:
  ProtoDryRunCallImpl() = default;

  void Init(const proto::Call& call) {
    cid_ = call.correlation_id();
    for (auto&& [k, v] : call.system_tags()) {
      sys_tags_[k] = v;
    }
    for (auto&& [k, v] : call.user_tags()) {
      user_tags_[k] = v;
    }
    for (auto&& e : call.incoming_pkts()) {
      incoming_pkts_.emplace_back(
          DryRunPacket{.time_since_start =
                           e.time_since_start() * std::chrono::nanoseconds(1),
                       .provider_ctx = e.provider_context(),
                       .system_ctx = e.system_context()});
    }
    for (auto&& e : call.outgoing_pkts()) {
      outgoing_pkts_.emplace_back(
          DryRunPacket{.time_since_start =
                           e.time_since_start() * std::chrono::nanoseconds(1),
                       .provider_ctx = e.provider_context(),
                       .system_ctx = e.system_context()});
    }
  }

  const std::string& GetCorrelationId() const noexcept override { return cid_; }
  const Tags& GetSystemTags() const noexcept override { return sys_tags_; }
  const Tags& GetUserTags() const noexcept override { return user_tags_; }
  const std::string& GetSystemContext() const noexcept override {
    return sys_ctx_;
  }

 protected:
  std::string cid_;
  Tags sys_tags_, user_tags_;
  std::string sys_ctx_;
  std::vector<DryRunPacket> incoming_pkts_, outgoing_pkts_;
};

}  // namespace detail

// This class helps you implementing `DryRunIncomingCall` in terms of
// `proto::Call`.
//
// NOT INTENDED FOR PUBLIC USE.
class ProtoDryRunIncomingCall
    : public detail::ProtoDryRunCallImpl<DryRunIncomingCall> {
 public:
  const std::vector<DryRunPacket>& GetIncomingPackets()
      const noexcept override {
    return incoming_pkts_;
  }

 protected:
  // Outgoing packets are only available if the they were filled into
  // `proto::Call` when `Init` was called.
  const std::vector<DryRunPacket>& GetOutgoingPackets() const noexcept {
    return outgoing_pkts_;
  }
};

// Same as above, but for `DryRunOutgoingCall`.
class ProtoDryRunOutgoingCall
    : public detail::ProtoDryRunCallImpl<DryRunOutgoingCall> {
 public:
  // This implementation is available only if incoming packets were available in
  // `proto::Call::incoming_pkts` when `Init` was called.
  Future<Expected<DryRunPacket, Status>> TryGetIncomingPacket(
      std::size_t index) override {
    if (index > incoming_pkts_.size()) {
      return Status{STATUS_EOF};
    }
    return incoming_pkts_[index];
  }

 protected:
  // These two accessors are only available if the corresponding fields were
  // filled when `Init` was called.
  const std::vector<DryRunPacket>& GetIncomingPackets() const noexcept {
    return incoming_pkts_;
  }
  const std::vector<DryRunPacket>& GetOutgoingPackets() const noexcept {
    return outgoing_pkts_;
  }
};

}  // namespace flare::binlog

#endif  // FLARE_RPC_BINLOG_UTIL_PROTO_DRY_RUNNER_H_
