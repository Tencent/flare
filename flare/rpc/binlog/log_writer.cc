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

#include "flare/rpc/binlog/log_writer.h"

#include "flare/base/internal/dpc.h"
#include "flare/rpc/binlog/tags.h"

using namespace std::literals;

namespace flare::binlog {

namespace {

template <class T, class U>
void EvaluateLazyPacketToReport(std::uint64_t start_tsc, T* from, U* to) {
  to->time_since_start = DurationFromTsc(start_tsc, from->tsc);
  if (from->dump_ctx) {
    to->dumper_context = from->dump_ctx.Evaluate();
  }
  if (from->prov_ctx) {
    to->provider_context = from->prov_ctx.Evaluate();
  }
  if (from->sys_ctx) {
    to->system_context = from->sys_ctx.Evaluate();
  }
}

}  // namespace

void IncomingCallWriter::Flush() {
  std::unordered_map<std::string, std::string> sys_tags;

  sys_tags[tags::kServiceName] = service_name_;
  sys_tags[tags::kOperationName] = operation_name_;
  sys_tags[tags::kHandlerUuid] = handler_uuid_.ToString();
  sys_tags[tags::kLocalPeer] = local_peer_.ToString();
  sys_tags[tags::kRemotePeer] = remote_peer_.ToString();
  sys_tags[tags::kInvocationStatus] = invocation_status_;

  std::unordered_map<std::string, std::string> user_tags{
      lazy_user_tags_.begin(), lazy_user_tags_.end()};

  std::vector<DumpingPacket> incoming_pkts, outgoing_pkts;
  for (auto&& e : lazy_incomings_) {
    EvaluateLazyPacketToReport(start_tsc_, &e, &incoming_pkts.emplace_back());
  }
  for (auto&& e : lazy_outgoings_) {
    EvaluateLazyPacketToReport(start_tsc_, &e, &outgoing_pkts.emplace_back());
  }

  reader_provider_->SetCorrelationId(correlation_id_);
  reader_provider_->SetTimestamps(start_ts_, finish_ts_);
  reader_provider_->SetSystemTags(std::move(sys_tags));
  reader_provider_->SetUserTags(std::move(user_tags));
  reader_provider_->SetLogs(std::move(logs_));
  reader_provider_->SetIncomingPackets(std::move(incoming_pkts));
  reader_provider_->SetOutgoingPackets(std::move(outgoing_pkts));
  reader_provider_->SetSystemContext(lazy_sys_ctx_ ? lazy_sys_ctx_.Evaluate()
                                                   : "");
}

void OutgoingCallWriter::Flush() {
  std::unordered_map<std::string, std::string> sys_tags;

  sys_tags[tags::kOperationName] = operation_name_;
  sys_tags[tags::kUri] = uri_;
  sys_tags[tags::kInvocationStatus] = invocation_status_;

  std::unordered_map<std::string, std::string> user_tags{
      lazy_user_tags_.begin(), lazy_user_tags_.end()};

  std::vector<DumpingPacket> incomings, outgoings;
  for (auto&& e : lazy_incomings_) {
    EvaluateLazyPacketToReport(start_tsc_, &e, &incomings.emplace_back());
  }
  for (auto&& e : lazy_outgoings_) {
    EvaluateLazyPacketToReport(start_tsc_, &e, &outgoings.emplace_back());
  }

  writer_provider_->SetCorrelationId(correlation_id_);
  writer_provider_->SetTimestamps(start_ts_, finish_ts_);
  writer_provider_->SetSystemTags(std::move(sys_tags));
  writer_provider_->SetUserTags(std::move(user_tags));
  writer_provider_->SetLogs(std::move(logs_));
  writer_provider_->SetIncomingPackets(std::move(incomings));
  writer_provider_->SetOutgoingPackets(std::move(outgoings));
  writer_provider_->SetSystemContext(lazy_sys_ctx_ ? lazy_sys_ctx_.Evaluate()
                                                   : "");
}

void LogWriter::Dump() {
  internal::QueueDpc([aborted = aborted_.load(std::memory_order_relaxed),
                      state = std::move(state_)]() mutable {
    if (!aborted) {
      state->incoming_.Flush();
      for (auto&& e : state->outgoings_) {
        e.Flush();
      }
      state->log->Dump();
    } else {
      state->log->Abort();
    }
  });
}

}  // namespace flare::binlog
