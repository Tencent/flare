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

#include "flare/net/hbase/hbase_service.h"

#include <chrono>
#include <memory>
#include <utility>

#include "flare/base/callback.h"
#include "flare/base/down_cast.h"
#include "flare/rpc/internal/fast_latch.h"
#include "flare/rpc/internal/rpc_metrics.h"
#include "flare/net/hbase/call_context.h"
#include "flare/net/hbase/hbase_server_controller.h"
#include "flare/net/hbase/hbase_server_protocol.h"
#include "flare/net/hbase/message.h"

using namespace std::literals;

namespace flare {

void HbaseService::AddService(google::protobuf::Service* service) {
  hbase::HbaseServerProtocol::RegisterService(service->GetDescriptor());
  services_[service->GetDescriptor()->name()] = service;

  auto desc = service->GetDescriptor();
  for (auto i = 0; i != desc->method_count(); ++i) {
    rpc::detail::RpcMetrics::Instance()->RegisterMethod(desc->method(i));
  }
}

const experimental::Uuid& HbaseService::GetUuid() const noexcept {
  static constexpr experimental::Uuid kUuid(
      "2C430A0F-E783-4A78-9E0E-5F414110EA01");
  return kUuid;
}

bool HbaseService::Inspect(const Message& message, const Controller& controller,
                           InspectionResult* result) {
  if (auto p = dyn_cast<hbase::HbaseRequest>(message); FLARE_LIKELY(p)) {
    auto ctx = cast<hbase::PassiveCallContext>(controller);
    result->method = ctx->method->full_name();
    return true;
  }
  return false;
}

bool HbaseService::ExtractCall(
    const std::string& serialized,
    const std::vector<std::string>& serialized_pkt_ctxs,
    ExtractedCall* extracted) {
  FLARE_LOG_ERROR_ONCE("{}: Not implemented.", __func__);
  return false;
}

StreamService::ProcessingStatus HbaseService::FastCall(
    std::unique_ptr<Message>* request,
    const FunctionView<std::size_t(const Message&)>& writer, Context* context) {
  auto req = cast<hbase::HbaseRequest>(**request);
  auto call_ctx = cast<hbase::PassiveCallContext>(context->controller);
  HbaseServerController ctlr;

  ctlr.SetRemotePeer(context->remote_peer);
  ctlr.SetRequestCellBlock(std::move(req)->cell_block);
  ctlr.SetConnectionHeader(call_ctx->conn_header);
  if (req->header.has_timeout()) {
    ctlr.SetTimeout(req->header.timeout() * 1ms);
  }

  auto service_iter = services_.find(call_ctx->service->name());
  if (service_iter == services_.end()) {
    FLARE_LOG_WARNING_EVERY_SECOND("Service [{}] is not found.",
                                   call_ctx->service->name());
    // For a given HBase connection, all request / response running on it is
    // associated with the same service. Given that what's request is unknown to
    // us, everything else on the connection won't be recognized by us either.
    // So we drop the connection.
    return ProcessingStatus::Corrupted;
  }

  // Call user's code synchronously.
  rpc::detail::FastLatch fast_latch;
  internal::LocalCallback done_callback([&] { fast_latch.count_down(); });
  service_iter->second->CallMethod(call_ctx->method, &ctlr,
                                   std::get<0>(req->body).Get(),
                                   call_ctx->response.get(), &done_callback);
  fast_latch.wait();

  // Build the response and send it back.
  hbase::HbaseResponse resp;
  resp.header.set_call_id(req->header.call_id());
  if (ctlr.Failed()) {
    *resp.header.mutable_exception() = ctlr.GetException();
  } else {
    resp.body = call_ctx->response.get();
    if (!ctlr.GetResponseCellBlock().Empty()) {
      resp.cell_block = ctlr.GetResponseCellBlock();
      resp.header.mutable_cell_block_meta()->set_length(
          resp.cell_block.ByteSize());
    }
  }
  // TODO(luobogao): We can do this early, once `done` is called.
  auto bytes = writer(resp);
  auto status = ctlr.Failed() ? 1 : 0;  // HBase does not use error code, so the
                                        // error value makes little sense.
  context->status = status;
  rpc::detail::RpcMetrics::Instance()->Report(
      call_ctx->method, status, ctlr.GetElapsedTime() / 1ms,
      context->incoming_packet_size, bytes);
  return ProcessingStatus::Processed;
}

StreamService::ProcessingStatus HbaseService::StreamCall(
    AsyncStreamReader<std::unique_ptr<Message>>* input_stream,
    AsyncStreamWriter<std::unique_ptr<Message>>* output_stream,
    Context* context) {
  return ProcessingStatus::Unexpected;
}

void HbaseService::Stop() {
  // NOTHING.
}

void HbaseService::Join() {
  // NOTHING.
}

}  // namespace flare
