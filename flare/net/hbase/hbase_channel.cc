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

#include "flare/net/hbase/hbase_channel.h"

#include <memory>
#include <utility>
#include <vector>

#include "flare/base/callback.h"
#include "flare/base/down_cast.h"
#include "flare/base/random.h"
#include "flare/base/string.h"
#include "flare/fiber/latch.h"
#include "flare/net/hbase/hbase_client_controller.h"
#include "flare/net/hbase/hbase_client_protocol.h"
#include "flare/net/hbase/message.h"
#include "flare/net/hbase/proto/rpc.pb.h"
#include "flare/rpc/internal/correlation_id.h"
#include "flare/rpc/internal/stream_call_gate.h"
#include "flare/rpc/internal/stream_call_gate_pool.h"
#include "flare/rpc/name_resolver/name_resolver.h"

using namespace std::literals;
using flare::rpc::internal::StreamCallGate;

namespace flare {

namespace {

const auto kScheme = "hbase"s;

bool ResolveAddress(std::string_view address, Endpoint* resolved) {
  static const std::string kPrefix = kScheme + "://";

  FLARE_CHECK(StartsWith(address, kPrefix),
              "`HbaseChannel` only accepts URI with scheme 'hbase'. URI "
              "provided: [{}].",
              address);

  // Server address is always specified as `host:port`. So we hardcode name
  // resolver `list` to resolve the address.
  auto ip_port = std::string(address.substr(kPrefix.size()));
  auto resolver = name_resolver_registry.Get("list");

  auto view = resolver->StartResolving(ip_port);
  if (!view) {
    return false;
  }
  std::vector<Endpoint> addrs;
  view->GetPeers(&addrs);
  FLARE_CHECK(addrs.size() <= 1,
              "More than one hosts is specified. `HbaseChannel` can only "
              "connect to exactly 1 server. URI provided: [{}].",
              address);

  if (addrs.empty()) {  // Resolution failed.
    return false;
  }
  *resolved = std::move(addrs.front());
  return true;
}

std::unique_ptr<StreamProtocol> CreateClientProtocolWithOptions(
    const HbaseChannel::Options& options) {
  hbase::ConnectionHeader conn_header;
  conn_header.set_service_name(options.service_name);
  conn_header.mutable_user_info()->set_effective_user(options.effective_user);
  if (!options.cell_block_codec.empty()) {
    conn_header.set_cell_block_codec_class(options.cell_block_codec);
  }
  if (!options.cell_block_compressor.empty()) {
    conn_header.set_cell_block_compressor_class(options.cell_block_compressor);
  }
  auto protocol = std::make_unique<hbase::HbaseClientProtocol>();
  protocol->InitializeHandshakeConfig(std::move(conn_header));
  return protocol;
}

RefPtr<StreamCallGate> CreateCallGate(const Endpoint& server,
                                      const HbaseChannel::Options& options) {
  auto gate = MakeRefCounted<StreamCallGate>();
  StreamCallGate::Options opts;
  opts.protocol = CreateClientProtocolWithOptions(options);
  opts.maximum_packet_size = options.maximum_packet_size;
  gate->Open(server, std::move(opts));
  if (!gate->Healthy()) {
    FLARE_LOG_WARNING_EVERY_SECOND("Failed to open call gate to [{}].",
                                   server.ToString());
    // Fall-through. We don't bother handling error here. Making RPC via an
    // unhealthy gate would raise an appropriate error.
  }
  return gate;
}

}  // namespace

bool HbaseChannel::Open(const std::string& address, const Options& options) {
  options_ = options;
  if (!ResolveAddress(address, &server_addr_)) {
    FLARE_LOG_WARNING_EVERY_SECOND("Cannot resolve HBase address [{}].",
                                   address);
    return false;
  }
  return true;
}

void HbaseChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                              google::protobuf::RpcController* controller,
                              const google::protobuf::Message* request,
                              google::protobuf::Message* response,
                              google::protobuf::Closure* done) {
  FLARE_CHECK_EQ(method->service()->name(), options_.service_name,
                 "The channel was opened for calling service [{}], you cannot "
                 "use it to call method [{}] on service [{}].",
                 options_.service_name, method->name(),
                 method->service()->name());

  bool blocking_call = !done;
  fiber::Latch latch(1);
  if (blocking_call) {
    FLARE_CHECK(!done);
    done = NewCallback([&] { latch.count_down(); });
  }
  CallMethodNonEmptyDone(method, controller, request, response, done);
  if (blocking_call) {
    latch.wait();
  }
}

void HbaseChannel::CallMethodNonEmptyDone(
    const google::protobuf::MethodDescriptor* method,
    google::protobuf::RpcController* controller,
    const google::protobuf::Message* request,
    google::protobuf::Message* response, google::protobuf::Closure* done) {
  auto ctlr = flare::down_cast<HbaseClientController>(controller);
  auto gate = GetCallGate();
  RefPtr gate_ref(ref_ptr, gate.Get());
  auto correlation_id = rpc::internal::NewRpcCorrelationId();

  ctlr->SetRemotePeer(gate_ref->GetEndpoint());

  // Initialize call context.
  auto call_ctx = ctlr->GetCallContext();
  call_ctx->method = method;
  call_ctx->response_ptr = response;
  call_ctx->client_controller = ctlr;

  // Initialize request.
  hbase::HbaseRequest msg;
  msg.body = request;
  msg.cell_block = ctlr->GetRequestCellBlock();
  msg.header.set_call_id(correlation_id);
  msg.header.set_method_name(method->name());
  msg.header.set_request_param(request != nullptr);
  if (auto x = msg.cell_block.ByteSize()) {
    msg.header.mutable_cell_block_meta()->set_length(x);
  }
  if (auto x = ctlr->GetPriority()) {
    msg.header.set_priority(x);
  }
  msg.header.set_timeout((ctlr->GetTimeout() - ReadSteadyClock()) / 1ms);

  // Make the call.
  auto on_completion = [=, gate = std::move(gate)](auto status, auto msg_ptr,
                                                   auto) mutable {
    gate.Close();
    // TODO(luobogao): Copy timestamps to `HbaseController`.
    if (status == StreamCallGate::CompletionStatus::Success) {
      auto&& resp = cast<hbase::HbaseResponse>(msg_ptr.get());
      if (resp->header.has_exception()) {
        ctlr->SetException(resp->header.exception());
      } else {
        if (!resp->cell_block.Empty()) {
          ctlr->SetResponseCellBlock(std::move(resp->cell_block));
        }
      }
    } else {
      HbaseException xcpt;
      xcpt.set_exception_class_name(
          status == StreamCallGate::CompletionStatus::IoError
              ? hbase::constants::kFatalConnectionException
              : hbase::constants::kCallTimeoutException);
      ctlr->SetException(std::move(xcpt));
    }
    done->Run();
  };

  auto call_args = object_pool::Get<StreamCallGate::FastCallArgs>();
  call_args->completion = std::move(on_completion);
  call_args->controller = call_ctx;
  if (auto ptr = fiber::ExecutionContext::Current()) {
    call_args->exec_ctx.Reset(ref_ptr, ptr);
  }
  gate_ref->FastCall(msg, std::move(call_args), ctlr->GetTimeout());
}

rpc::internal::StreamCallGateHandle HbaseChannel::GetCallGate() {
  static const auto gate_pool =
      rpc::internal::GetGlobalStreamCallGatePool(kScheme);

  return gate_pool->GetOrCreateShared(server_addr_, false, [&] {
    return CreateCallGate(server_addr_, options_);
  });
}

}  // namespace flare
