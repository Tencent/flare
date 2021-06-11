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

#include "flare/rpc/binlog/text_only/dumper.h"

#include <chrono>
#include <memory>
#include <string>

#include "gflags/gflags.h"
#include "jsoncpp/json.h"
#include "protobuf/util/json_util.h"

#include "flare/base/chrono.h"
#include "flare/base/logging.h"
#include "flare/base/string.h"
#include "flare/net/http/http_request.h"
#include "flare/net/http/http_response.h"
#include "flare/net/http/packet_desc.h"
#include "flare/rpc/binlog/testing.h"
#include "flare/rpc/binlog/text_only/binlog.pb.h"
#include "flare/rpc/binlog/util/easy_dumping_log.h"
#include "flare/rpc/binlog/util/proto_dumper.h"

using namespace std::literals;

DEFINE_string(flare_binlog_text_only_dumper_filename, "../log/rpc_dump.txt",
              "Path to file for dumping RPCs.");

namespace flare::binlog {

namespace {

// Being slow does not matter, this dumper is for debugging purpose anyway.
std::string ToJson(const google::protobuf::Message& msg) {
  std::string result;
  FLARE_CHECK(google::protobuf::util::MessageToJsonString(msg, &result).ok());
  Json::Value jsv;
  FLARE_CHECK(Json::Reader().parse(result, jsv));
  return Json::StyledWriter().write(jsv);
}

std::string CapturePacket(const PacketDesc& packet) {
  std::string s;
  if (auto p = dyn_cast<ProtoPacketDesc>(packet)) {
    if (p->message.index() == 0) {
      s = std::get<0>(p->message)->DebugString();
    } else {
      s = "(raw bytes message)";
    }
  } else if (auto p = dyn_cast<TestingPacketDesc>(packet)) {
    s = p->str;
  } else if (auto p = dyn_cast<http::PacketDesc>(packet)) {
    if (p->message.index() == 0) {
      s = *std::get<0>(p->message)->body();
    } else {
      s = *std::get<1>(p->message)->body();
    }
  } else {
    s = "(unknown packet type)";
  }
  return s;
}

class TextOnlyCall : public ProtoDumpingCall {
 public:
  void CaptureIncomingPacket(
      const PacketDesc& packet, experimental::LazyEval<std::any>* dumper_ctx,
      experimental::LazyEval<std::string>* prov_ctx) override {
    *prov_ctx = CapturePacket(packet);
  }

  void CaptureOutgoingPacket(
      const PacketDesc& packet, experimental::LazyEval<std::any>* dumper_ctx,
      experimental::LazyEval<std::string>* prov_ctx) override {
    *prov_ctx = CapturePacket(packet);
  }
};

// Represents an entire RPC log being dumped.
class TextOnlyLog : public EasyDumpingLog<TextOnlyCall> {
 public:
  explicit TextOnlyLog(TextOnlyDumper* dumper) : dumper_(dumper) {}

  void Dump() override {
    text_only::Log log;
    *log.mutable_incoming_call() = incoming_call_.GetMessage();
    for (auto&& e : outgoing_calls_) {
      *log.add_outgoing_calls() = e.GetMessage();
    }
    dumper_->Write(ToJson(log));
  }

 private:
  TextOnlyDumper* dumper_;
};

}  // namespace

TextOnlyDumper::TextOnlyDumper(const Options& options)
    : options_(options), dumping_to_(options_.filename) {
  FLARE_CHECK(dumping_to_, "Failed to open [{}] for dumping RPCs.",
              options_.filename);
  FLARE_LOG_WARNING(
      "Text-only binlog dumper is being used, performance will suffer.");
}

std::unique_ptr<DumpingLog> TextOnlyDumper::StartDumping() {
  return std::make_unique<TextOnlyLog>(this);
}

void TextOnlyDumper::Write(const std::string& entry) {
  // Performance does not matter much here.
  std::scoped_lock _(dump_lock_);
  dumping_to_ << entry << std::flush;
}

FLARE_RPC_BINLOG_REGISTER_DUMPER("text_only", [] {
  return std::make_unique<TextOnlyDumper>(TextOnlyDumper::Options{
      .filename = FLAGS_flare_binlog_text_only_dumper_filename});
});

}  // namespace flare::binlog
