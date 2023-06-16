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

#ifndef FLARE_RPC_BINLOG_LOG_WRITER_H_
#define FLARE_RPC_BINLOG_LOG_WRITER_H_

#include <atomic>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "flare/base/experimental/uuid.h"
#include "flare/rpc/binlog/dumper.h"

namespace flare::binlog {

// This class helps dumping incoming call.
class IncomingCallWriter {
 public:
  void SetProvider(DumpingCall* provider) { reader_provider_ = provider; }

  // Uniquely identifies a call.
  void SetCorrelationId(std::string cid) noexcept {
    correlation_id_ = std::move(cid);
  }

  // These names are for exposition purpose only. They may change without
  // further notice.
  void SetServiceName(std::string name) noexcept {
    service_name_ = std::move(name);
  }
  void SetOperationName(std::string name) noexcept {
    operation_name_ = std::move(name);
  }

  // Handler of `StreamService` handled this call.
  void SetHandlerUuid(const experimental::Uuid& uuid) noexcept {
    handler_uuid_ = uuid;
  }

  // Timestamps.
  void SetStartTimestamp(std::chrono::steady_clock::time_point ts) noexcept {
    start_ts_ = ts;
  }
  void SetFinishTimestamp(std::chrono::steady_clock::time_point ts) noexcept {
    finish_ts_ = ts;
  }

  // Not all protocols uses numeric invocation status (notably HBase), so we use
  // string here for representing it.
  void SetInvocationStatus(std::string invocation_status) noexcept {
    invocation_status_ = std::move(invocation_status);
  }

  // Peer addresses.
  void SetLocalPeer(const Endpoint& peer) { local_peer_ = peer; }
  void SetRemotePeer(const Endpoint& peer) { remote_peer_ = peer; }

  // If a key is set for multiple times, the last one wins.
  void SetUserTag(std::string key, std::string value) {
    lazy_user_tags_.emplace_back(std::move(key), std::move(value));
  }

  // For debugging purpose only.
  void AddLog(std::string s) { logs_.push_back(std::move(s)); }

  // Saves whatever interesting to the framework, it's opaque to binlog
  // provider.
  void SetSystemContext(experimental::LazyEval<std::string> sys_ctx) {
    lazy_sys_ctx_ = std::move(sys_ctx);
  }

  // `SetProviderContext`?

  // Called for each incoming / outgoing packet (i.e., request, response).
  //
  // Calls to this method MUST BE SERIALIZED. I can hardly see a reason why two
  // packets can be added simultaneously (how can these two packets be ordered
  // then?).
  void AddIncomingPacket(const PacketDesc& packet,
                         experimental::LazyEval<std::string> sys_ctx = {}) {
    lazy_incomings_.emplace_back().sys_ctx = std::move(sys_ctx);
    reader_provider_->CaptureIncomingPacket(packet,
                                            &lazy_incomings_.back().dump_ctx,
                                            &lazy_incomings_.back().prov_ctx);
  }
  void AddOutgoingPacket(const PacketDesc& packet,
                         experimental::LazyEval<std::string> sys_ctx = {}) {
    lazy_outgoings_.emplace_back().sys_ctx = std::move(sys_ctx);
    reader_provider_->CaptureOutgoingPacket(packet,
                                            &lazy_outgoings_.back().dump_ctx,
                                            &lazy_outgoings_.back().prov_ctx);
  }

  // Evalutes all deferred evaluations and flush the data to the provider.
  //
  // This is called automatically by the framework, you shouldn't call it.
  void Flush();

 private:
  struct LazyPacket {
    std::uint64_t tsc = ReadTsc();
    experimental::LazyEval<std::any> dump_ctx;  // Not serialized.
    experimental::LazyEval<std::string> prov_ctx, sys_ctx;
  };

  DumpingCall* reader_provider_{};
  std::uint64_t start_tsc_ = ReadTsc();

  // These attributes are serialized to `system_tags_` on `Finalize()`.
  std::string service_name_;
  std::string operation_name_;
  experimental::Uuid handler_uuid_;
  std::string invocation_status_;
  Endpoint local_peer_, remote_peer_;

  // Evaluated on `Finalize()`.
  std::vector<LazyPacket> lazy_incomings_, lazy_outgoings_;
  experimental::LazyEval<std::string> lazy_sys_ctx_;
  // We're using vector instead of map for perf. reasons. This vector is
  // unique-d on `Finalize()`.
  std::vector<std::pair<std::string, std::string>> lazy_user_tags_;

  std::string correlation_id_;
  std::unordered_map<std::string_view, std::string> system_tags_;
  std::chrono::steady_clock::time_point start_ts_, finish_ts_;
  std::vector<std::string> logs_;
};

// This class helps us to write an outgoing call.
class OutgoingCallWriter {
 public:
  void SetProvider(DumpingCall* provider) { writer_provider_ = provider; }

  // Set attributes on this call. These attributes are passed to the
  // implementation on `Finalize()`.
  void SetCorrelationId(std::string cid) noexcept {
    correlation_id_ = std::move(cid);
  }
  // Not sure if service name makes sense here.
  void SetOperationName(std::string name) noexcept {
    operation_name_ = std::move(name);
  }
  void SetUri(std::string uri) noexcept { uri_ = std::move(uri); }
  void SetStartTimestamp(std::chrono::steady_clock::time_point ts) noexcept {
    start_ts_ = ts;
  }
  void SetFinishTimestamp(std::chrono::steady_clock::time_point ts) noexcept {
    finish_ts_ = ts;
  }
  void SetInvocationStatus(std::string invocation_status) noexcept {
    invocation_status_ = std::move(invocation_status);
  }
  void SetUserTag(std::string key, std::string value) {
    lazy_user_tags_.emplace_back(std::move(key), std::move(value));
  }
  void AddLog(std::string s) { logs_.push_back(std::move(s)); }
  void SetSystemContext(experimental::LazyEval<std::string> sys_ctx) {
    lazy_sys_ctx_ = std::move(sys_ctx);
  }

  // Note that "outgoing packet" is the request we send.
  void AddOutgoingPacket(const PacketDesc& packet,
                         experimental::LazyEval<std::string> sys_ctx = {}) {
    lazy_outgoings_.emplace_back().sys_ctx = std::move(sys_ctx);
    writer_provider_->CaptureIncomingPacket(packet,
                                            &lazy_outgoings_.back().dump_ctx,
                                            &lazy_outgoings_.back().prov_ctx);
  }
  void AddIncomingPacket(const PacketDesc& packet,
                         experimental::LazyEval<std::string> sys_ctx = {}) {
    lazy_incomings_.emplace_back().sys_ctx = std::move(sys_ctx);
    writer_provider_->CaptureOutgoingPacket(packet,
                                            &lazy_incomings_.back().dump_ctx,
                                            &lazy_incomings_.back().prov_ctx);
  }

  // Evalutes all deferred evaluations and write all data to the provider.
  //
  // This is called automatically by the framework, you shouldn't call it.
  void Flush();

 private:
  struct LazyPacket {
    std::uint64_t tsc = ReadTsc();
    experimental::LazyEval<std::any> dump_ctx;
    experimental::LazyEval<std::string> prov_ctx, sys_ctx;
  };

  DumpingCall* writer_provider_{};
  std::uint64_t start_tsc_ = ReadTsc();

  std::string operation_name_;
  std::string uri_;
  std::string invocation_status_;
  std::vector<std::pair<std::string, std::string>> lazy_user_tags_;
  experimental::LazyEval<std::string> lazy_sys_ctx_;
  std::vector<LazyPacket> lazy_incomings_, lazy_outgoings_;

  std::chrono::steady_clock::time_point start_ts_, finish_ts_;

  std::string correlation_id_;
  std::vector<std::string> logs_;
};

// Helper class for constructing RPC binlog and dumping it via `Dumper`.
//
// NOT thread-safe unless otherwise stated.
class LogWriter {
 public:
  explicit LogWriter(Dumper* dumper) : state_{std::make_unique<State>()} {
    state_->log = dumper->StartDumping();
    state_->incoming_.SetProvider(state_->log->GetIncomingCall());
  }

  bool Dumping() const noexcept {
    return !aborted_.load(std::memory_order_relaxed);
  }

  // Wrapper for incoming call.
  IncomingCallWriter* GetIncomingCall() noexcept { return &state_->incoming_; }

  // Start a new outgoing call.
  //
  // Thread-safe.
  OutgoingCallWriter* StartOutgoingCall() {
    std::scoped_lock _(state_->lock_);
    auto ptr = state_->log->StartOutgoingCall();
    if (ptr) {
      state_->outgoings_.emplace_back().SetProvider(ptr);
      return &state_->outgoings_.back();
    } else {
      return nullptr;  // The implementation is not interested in capturing
                       // outgoing calls.
    }
  }

  // Abort this log. Something bad happened.
  void Abort() noexcept { aborted_.store(true, std::memory_order_relaxed); }

  // Dump the call (if it's indeed sampled). This method is completed
  // asynchronously in background thread.
  void Dump();

 private:
  struct State {
    std::unique_ptr<DumpingLog> log;
    IncomingCallWriter incoming_;
    std::mutex lock_;
    std::list<OutgoingCallWriter> outgoings_;
  };
  std::atomic<bool> aborted_{false};
  std::unique_ptr<State> state_;
};

}  // namespace flare::binlog

#endif  // FLARE_RPC_BINLOG_LOG_WRITER_H_
