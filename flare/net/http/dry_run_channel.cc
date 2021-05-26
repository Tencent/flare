// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "flare/net/http/dry_run_channel.h"

#include "flare/base/internal/lazy_init.h"
#include "flare/base/logging.h"
#include "flare/init/on_init.h"
#include "flare/net/http/packet_desc.h"
#include "flare/rpc/binlog/tags.h"
#include "flare/rpc/internal/session_context.h"
#include "flare/rpc/protocol/http/binlog.pb.h"

namespace flare::http {
namespace {

FLARE_ON_INIT(0, [] {
  detail::RegisterDryRunHttpChannel(internal::LazyInit<DryRunChannel>());
});

std::optional<std::string> GetHttpRequestUriFromUrl(const std::string& url) {
  std::optional<std::string> uri;
  auto start_pos = url.find("://");
  if (start_pos != std::string::npos) {
    start_pos += 3;
    auto end_pos = url.find("/", start_pos);
    if (end_pos != std::string::npos) {
      return url.substr(end_pos);
    }
  }
  return std::nullopt;
}

void AsyncCall(
    const std::string& url, const HttpRequest& request,
    const HttpClient::RequestOptions& request_options,
    HttpClient::ResponseInfo* response_info,
    Function<void(Expected<HttpResponse, HttpClient::ErrorCode>)>&& done) {
  FLARE_CHECK(rpc::session_context->binlog.dry_runner);
  auto cid = detail::GetHttpBinlogCorrelationId(
      url, request_options.binlog_correlation_id);
  auto call =
      rpc::session_context->binlog.dry_runner->TryStartOutgoingCall(cid);
  if (!call) {
    FLARE_LOG_WARNING_EVERY_SECOND(
        "Unexpected Http. Are you making calls to a new backend?");
    done(HttpClient::ErrorCode::ERROR_DRY_RUN);
    return;
  }

  auto cb = [call, done = std::move(done)](
                Expected<binlog::DryRunPacket, Status> packet) {
    if (!packet) {
      FLARE_LOG_WARNING_EVERY_SECOND("`GetIncomingPacket` failed with: {}",
                                     packet.error().ToString());
      done(HttpClient::ErrorCode::ERROR_DRY_RUN);
      return;
    }

    auto&& tags = (*call)->GetSystemTags();
    auto it = tags.find(flare::binlog::tags::kInvocationStatus);
    if (it == tags.end()) {
      FLARE_LOG_WARNING_EVERY_SECOND("Can't find invocation status");
      done(HttpClient::ErrorCode::ERROR_DRY_RUN);
      return;
    }
    auto invocation_status = TryParse<uint32_t>(it->second);
    if (!invocation_status) {
      FLARE_LOG_WARNING_EVERY_SECOND("Invocation status invalid {}",
                                     it->second);
      done(HttpClient::ErrorCode::ERROR_DRY_RUN);
      return;
    }

    http::SerializedClientPacket result;
    if (!result.ParseFromString(packet->system_ctx)) {
      FLARE_LOG_ERROR_EVERY_SECOND(
          "Unexpected: Failed to parse `OutgoingCall.context`. Incompatible "
          "binlog replayed?");
      done(HttpClient::ErrorCode::ERROR_DRY_RUN);
      return;
    }

    if (invocation_status != 0) {
      done(static_cast<HttpClient::ErrorCode>(*invocation_status));
      return;
    }
    HttpResponse response;
    response.set_status(static_cast<HttpStatus>(result.status()));
    response.set_version(static_cast<HttpVersion>(result.version()));
    for (auto&& h : result.headers()) {
      response.headers()->Append(h.key(), h.value());
    }
    response.set_body(result.body());
    done(response);
  };
  (*call)->CaptureOutgoingPacket(http::PacketDesc(request));
  (*call)
      ->TryGetIncomingPacketEnumlatingDelay(0 /* First response */)
      .Then(std::move(cb));
}

}  // namespace

void DryRunChannel::AsyncGet(
    const std::string& url, const HttpClient::Options& opts,
    const HttpClient::RequestOptions& request_options,
    HttpClient::ResponseInfo* response_info,
    Function<void(Expected<HttpResponse, HttpClient::ErrorCode>)>&& done) {
  HttpRequest request;
  request.set_method(HttpMethod::Get);
  if (auto uri = GetHttpRequestUriFromUrl(url); uri) {
    request.set_uri(*uri);
  }
  AsyncCall(url, request, request_options, response_info, std::move(done));
}

void DryRunChannel::AsyncPost(
    const std::string& url, const HttpClient::Options& opts, std::string data,
    const HttpClient::RequestOptions& request_options,
    HttpClient::ResponseInfo* response_info,
    Function<void(Expected<HttpResponse, HttpClient::ErrorCode>)>&& done) {
  HttpRequest request;
  request.set_method(HttpMethod::Post);
  if (auto uri = GetHttpRequestUriFromUrl(url); uri) {
    request.set_uri(*uri);
  }
  request.set_body(data);
  AsyncCall(url, request, request_options, response_info, std::move(done));
}

void DryRunChannel::AsyncRequest(
    const std::string& protocol, const std::string& host,
    const HttpClient::Options& opts, const HttpRequest& request,
    const HttpClient::RequestOptions& request_options,
    HttpClient::ResponseInfo* response_info,
    Function<void(Expected<HttpResponse, HttpClient::ErrorCode>)>&& done) {
  auto url = protocol + "://" + host + request.uri();
  AsyncCall(url, request, request_options, response_info, std::move(done));
}

}  // namespace flare::http
