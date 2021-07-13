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

#include "flare/rpc/binlog/log_reader.h"

#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "flare/base/experimental/uuid.h"
#include "flare/base/string.h"
#include "flare/fiber/timer.h"
#include "flare/rpc/binlog/tags.h"

namespace flare::binlog {

namespace {

template <class T, class U>
std::optional<T> TryGet(const U& tags, const std::string& key) {
  auto iter = tags.find(key);
  if (iter == tags.end()) {
    return std::nullopt;
  }
  return TryParse<T>(iter->second);
}

}  // namespace

bool IncomingCallReader::InitializeWithProvider(DryRunIncomingCall* provider) {
  provider_ = provider;
  auto&& sys_tags = provider->GetSystemTags();
  auto uuid = TryGet<experimental::Uuid>(sys_tags, tags::kHandlerUuid);

  if (!uuid) {
    return false;
  }

  handler_uuid_ = *uuid;
  return true;
}

const experimental::Uuid& IncomingCallReader::GetHandlerUuid() const noexcept {
  return handler_uuid_;
}

const std::string& IncomingCallReader::GetCorrelationId() const noexcept {
  return provider_->GetCorrelationId();
}

const IncomingCallReader::Tags& IncomingCallReader::GetUserTags()
    const noexcept {
  return provider_->GetUserTags();
}

const IncomingCallReader::Tags& IncomingCallReader::GetSystemTags()
    const noexcept {
  return provider_->GetSystemTags();
}

const std::string& IncomingCallReader::GetSystemContext() const noexcept {
  return provider_->GetSystemContext();
}

const std::vector<DryRunPacket>& IncomingCallReader::GetIncomingPackets()
    const noexcept {
  return provider_->GetIncomingPackets();
}

void IncomingCallReader::CaptureOutgoingPacket(const PacketDesc& packet) {
  provider_->CaptureOutgoingPacket(packet);
}

bool OutgoingCallReader::InitializeWithProvider(DryRunOutgoingCall* provider) {
  provider_ = provider;
  // Nothing yet.
  return true;
}

const std::string& OutgoingCallReader::GetCorrelationId() const noexcept {
  return provider_->GetCorrelationId();
}

const OutgoingCallReader::Tags& OutgoingCallReader::GetUserTags()
    const noexcept {
  return provider_->GetUserTags();
}

const OutgoingCallReader::Tags& OutgoingCallReader::GetSystemTags()
    const noexcept {
  return provider_->GetSystemTags();
}

const std::string& OutgoingCallReader::GetSystemContext() const noexcept {
  return provider_->GetSystemContext();
}

void OutgoingCallReader::CaptureOutgoingPacket(const PacketDesc& packet) {
  provider_->CaptureOutgoingPacket(packet);
}

void OutgoingCallReader::SetStartTimestamp(
    std::chrono::steady_clock::time_point ts) {
  start_ts_ = ts;
}

Future<Expected<DryRunPacket, Status>>
OutgoingCallReader::TryGetIncomingPacketEnumlatingDelay(std::size_t index) {
  auto cb = [start = ReadSteadyClock()](Expected<DryRunPacket, Status> exp) {
    if (!exp) {
      return MakeReadyFuture(std::move(exp));
    }

    auto expected_delay = std::max<std::chrono::nanoseconds>(
        std::chrono::nanoseconds(0),
        ReadSteadyClock() - start + exp->time_since_start);
    if (expected_delay == std::chrono::nanoseconds(0)) {
      // Well `GetIncomingPacket` itself has consumed long enough, so we
      // satisfy the call immediately.
      return MakeReadyFuture(std::move(exp));
    }

    // Let's emulate the delay then.
    Promise<Expected<DryRunPacket, Status>> p;
    auto f = p.GetFuture();
    fiber::SetDetachedTimer(ReadSteadyClock() + expected_delay,
                            [p = std::move(p), exp = std::move(exp)]() mutable {
                              p.SetValue(std::move(*exp));
                            });
    return f;
  };
  return provider_->TryGetIncomingPacket(index).Then(cb);
}

bool LogReader::InitializeWithProvider(
    std::unique_ptr<DryRunContext> provider) {
  provider_ = std::move(provider);
  return incoming_.InitializeWithProvider(provider_->GetIncomingCall());
}

IncomingCallReader* LogReader::GetIncomingCall() {
  return &incoming_;  // Initialized in `InitializeWithProvider`.
}

Expected<OutgoingCallReader*, Status> LogReader::TryStartOutgoingCall(
    const std::string& correlation_id) {
  std::scoped_lock _(lock_);
  auto ptr = provider_->TryGetOutgoingCall(correlation_id);
  if (!ptr) {
    return ptr.error();
  }
  FLARE_CHECK(*ptr);
  if (!outgoings_.emplace_back().InitializeWithProvider(*ptr)) {
    FLARE_LOG_WARNING_EVERY_SECOND("Failed to initialize outgoing call [{}].",
                                   correlation_id);
    return Status{STATUS_INTERNAL_ERROR};
  }
  return &outgoings_.back();
}

void LogReader::SetInvocationStatus(std::string status) {
  provider_->SetInvocationStatus(std::move(status));
}

void LogReader::WriteReport(NoncontiguousBuffer* buffer) const {
  provider_->WriteReport(buffer);
}

}  // namespace flare::binlog
