// Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
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

#define FLARE_HTTP_FILTER_SUPPRESS_INCLUDE_WARNING
#define FLARE_HTTP_HANDLER_SUPPRESS_INCLUDE_WARNING

#include "flare/rpc/protocol/http/service.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "flare/base/buffer/compression_output_stream.h"
#include "flare/base/compression.h"
#include "flare/base/compression/util.h"
#include "flare/base/logging.h"
#include "flare/net/http/packet_desc.h"
#include "flare/rpc/internal/session_context.h"
#include "flare/rpc/protocol/http/binlog.pb.h"
#include "flare/rpc/protocol/http/message.h"

using namespace std::literals;

namespace flare::http {

namespace {

void CompressResponseIfNecessary(const HttpRequest& request,
                                 HttpResponse* response) {
  if (!response->body_size()) {
    return;
  }
  if (auto opt = request.headers()->TryGet(kAcceptEncoding);
      opt && !response->headers()->TryGet(kContentEncoding)) {
    // Ex: gzip;q=1.0, identity; q=0.5, *;q=0
    // We don't consider q here.
    auto encodings = Split(*opt, ",");
    for (auto&& encoding_and_q : encodings) {
      std::string_view encoding_sv = encoding_and_q;
      if (auto pos = encoding_and_q.find(";"); pos != std::string_view::npos) {
        encoding_sv = encoding_and_q.substr(0, pos);
      }
      std::string encoding = std::string(Trim(encoding_sv));
      auto compressor = MakeCompressor(encoding);
      if (!compressor) {
        // Don't support this encoding, try next.
        continue;
      }
      std::optional<NoncontiguousBuffer> compressed_body;
      if (auto nb = response->noncontiguous_body(); nb) {
        compressed_body = Compress(compressor.get(), *nb);
      } else {
        std::string* body = response->body();
        FLARE_CHECK(!body->empty(),
                    "Now that body size is not 0, body should not be empty.");
        compressed_body = Compress(compressor.get(), *body);
      }

      if (compressed_body) {
        response->set_body(std::move(*compressed_body));
        response->headers()->Set(std::string(kContentLength),
                                 std::to_string(response->body_size()));
        response->headers()->Append(std::string(kContentEncoding), encoding);
        return;
      } else {
        // Try next encoding.
        FLARE_LOG_WARNING_EVERY_SECOND("Compressor error with {} length {}",
                                       encoding, response->body_size());
      }
    }
  }
}

void FillMissingHeaders(const HttpRequest& request, HttpResponse* response) {
  if (!response->headers()->TryGet(kConnection)) {
    if (auto opt = request.headers()->TryGet(kConnection); opt) {
      response->headers()->Append(std::string(kConnection), std::string(*opt));
    } else {
      // HTTP/1.0 defaults to short connection.
      response->headers()->Append(
          std::string(kConnection),
          request.version() == HttpVersion::V_1_1 ? "keep-alive" : "close");
    }
  }
  int status = static_cast<int>(response->status());
  // A server MUST NOT send a Content-Length header field in any response with a
  // status code of 1xx (Informational) or 204 (No Content).
  if (response->status() == HttpStatus::NoContent ||
      (status >= 100 && status < 200)) {
    FLARE_CHECK_EQ(response->body_size(), 0,
                   "Http status {} should not contains body.", status);
    if (response->headers()->TryGet(kContentLength)) {
      FLARE_LOG_WARNING_EVERY_SECOND(
          "Server MUST NOT send a Content-Length header field in any response "
          "with a status of {}, we remove it.",
          status);
      response->headers()->Remove(kContentLength);
    }
  } else if (!response->headers()->TryGet(kContentLength)) {
    response->headers()->Append(std::string(kContentLength),
                                std::to_string(response->body_size()));
  }
  CompressResponseIfNecessary(request, response);
}

std::string_view GetUriPath(const std::string_view& uri) {
  auto pos = uri.find_first_of('?');
  return pos != std::string_view::npos ? uri.substr(0, pos) : uri;
}

}  // namespace

Service::Service() {
  default_handler_ = NewHttpHandler([](auto&& req, auto&& resp, auto&& ctx) {
    GenerateDefaultResponsePage(HttpStatus::NotFound, resp);
  });
}

void Service::AddFilter(MaybeOwning<HttpFilter> filter) {
  filters_.push_back(std::move(filter));
}

void Service::AddHandler(std::string path, MaybeOwning<HttpHandler> handler) {
  FLARE_CHECK(exact_paths_.find(path) == exact_paths_.end(),
              "Path [{}] has already been registered.", path);
  exact_paths_storage_.push_back(std::move(path));
  exact_paths_[exact_paths_storage_.back()] = std::move(handler);
}

void Service::AddHandler(std::regex path_regex,
                         MaybeOwning<HttpHandler> handler) {
  // I'm not sure if we can check for duplicate here.
  regex_paths_.push_back(std::pair(std::move(path_regex), std::move(handler)));
}

void Service::AddPrefixHandler(std::string path_prefix,
                               MaybeOwning<HttpHandler> handler) {
  prefix_paths_.push_back(
      std::pair(std::move(path_prefix), std::move(handler)));
}

void Service::SetDefaultHandler(MaybeOwning<HttpHandler> handler) {
  default_handler_ = std::move(handler);
}

const experimental::Uuid& Service::GetUuid() const noexcept {
  static constexpr experimental::Uuid kUuid(
      "FF754BCC-3E51-4ECB-8DE4-67F6A4A6AA61");
  return kUuid;
}

bool Service::Inspect(const Message& message, const Controller& controller,
                      InspectionResult* result) {
  if (auto p = dyn_cast<HttpRequestMessage>(message); FLARE_LIKELY(p)) {
    result->method = p->request()->uri();
    return true;
  }
  return false;
}

bool Service::ExtractCall(const std::string& serialized,
                          const std::vector<std::string>& serialized_pkt_ctxs,
                          ExtractedCall* extracted) {
  if (serialized_pkt_ctxs.size() != 1) {
    FLARE_LOG_ERROR_ONCE("Unexpected: Streaming HTTP request?");
    return false;
  }
  SerializedServerPacket packet;
  if (!packet.ParseFromString(serialized_pkt_ctxs[0])) {
    FLARE_LOG_WARNING_EVERY_SECOND("Failed to deserialize HTTP binlog.");
    return false;
  }
  auto request = std::make_unique<HttpRequestMessage>();
  request->request()->set_method(static_cast<HttpMethod>(packet.method()));
  request->request()->set_uri(packet.uri());
  request->request()->set_version(static_cast<HttpVersion>(packet.version()));
  for (auto&& h : packet.headers()) {
    request->headers()->Append(h.key(), h.value());
  }
  *request->body() = packet.body();

  extracted->messages.push_back(std::move(request));
  extracted->controller = nullptr;  // Not used by us.
  return true;
}

StreamService::ProcessingStatus Service::FastCall(
    std::unique_ptr<Message>* request,
    const FunctionView<std::size_t(const Message&)>& writer,
    StreamService::Context* context) {
  auto http_request_msg = cast<HttpRequestMessage>(**request);
  auto&& http_request = http_request_msg->request();
  HttpResponseMessage http_response_msg;
  auto&& http_response = http_response_msg.response();
  HttpServerContext http_context;

  http_context.remote_peer = context->remote_peer;
  http_context.received_at = TimestampFromTsc(context->received_tsc);
  http_context.dispatched_at = TimestampFromTsc(context->dispatched_tsc);
  http_context.parsed_at = TimestampFromTsc(context->parsed_tsc);

  // Note that any binlog of `request` must be captured here, as the request can
  // be mutated by the filters soon.

  // Default to success, in the same way as `RpcServerController`.
  http_response->set_version(http_request->version());
  http_response->set_status(HttpStatus::OK);

  auto action = RunFilters(http_request, http_response, &http_context);
  if (action == HttpFilter::Action::Drop) {
    FLARE_VLOG(10, "HTTP request dropped by filter.");
    return ProcessingStatus::Dropped;
  } else if (action == HttpFilter::Action::KeepProcessing) {
    RunHandler(*http_request, http_response, &http_context);
  } else {
    FLARE_VLOG(10, "HTTP request handled by filter.");
    FLARE_CHECK(action == HttpFilter::Action::EarlyReturn);
    // Nothing to do then, whatever filled by the filter into `response` is
    // respected.
  }

  // Not sure if this should be called if filter returned `EarlyReturn`.
  FillMissingHeaders(*http_request, http_response);
  context->status = underlying_value(http_response_msg.response()->status());
  writer(http_response_msg);

  CompleteBinlogPostOperation(*http_request, *http_response, http_context);

  return IEquals(http_response->headers()->TryGet("Connection").value_or(""),
                 "keep-alive")
             ? ProcessingStatus::Processed
             : ProcessingStatus::Completed;
}

void Service::CompleteBinlogPostOperation(const HttpRequest& req,
                                          const HttpResponse& resp,
                                          const HttpServerContext& context) {
  if (auto&& dumper = rpc::session_context->binlog.dumper;
      FLARE_UNLIKELY(dumper && dumper->Dumping())) {
    if (context.abort_binlog_capture) {
      dumper->Abort();
    } else {
      auto&& incoming = dumper->GetIncomingCall();
      for (auto&& [k, v] : context.binlog_tags) {
        incoming->SetUserTag(k, v);
      }

      SerializedServerPacket serialized;
      serialized.set_method(static_cast<int>(req.method()));
      serialized.set_uri(req.uri());
      serialized.set_version(static_cast<int>(req.version()));
      for (auto&& [k, v] : *req.headers()) {
        auto h = serialized.add_headers();
        h->set_key(std::string(k));
        h->set_value(std::string(v));
      }
      serialized.set_body(*req.body());
      incoming->AddIncomingPacket(http::PacketDesc(req),
                                  serialized.SerializeAsString());
      incoming->AddOutgoingPacket(http::PacketDesc(resp));
    }
  } else if (auto&& dry_runner = rpc::session_context->binlog.dry_runner;
             FLARE_UNLIKELY(dry_runner)) {
    dry_runner->GetIncomingCall()->CaptureOutgoingPacket(
        http::PacketDesc(resp));
    dry_runner->SetInvocationStatus(
        std::to_string(static_cast<int>(resp.status())));
  }
}

StreamService::ProcessingStatus Service::StreamCall(
    AsyncStreamReader<std::unique_ptr<Message>>* input_stream,
    AsyncStreamWriter<std::unique_ptr<Message>>* output_stream,
    StreamService::Context* context) {
  return ProcessingStatus::Unexpected;
}

void Service::Stop() {}
void Service::Join() {}

HttpHandler* Service::FindHandler(std::string_view uri) {
  auto path = GetUriPath(uri);

  if (auto iter = exact_paths_.find(path); iter != exact_paths_.end()) {
    return &*iter->second;
  }
  for (auto&& [prefix, h] : prefix_paths_) {
    // For prefix /inpect/rpc
    // /inspect/rpc/a1 and /inspect/rpc?q=1 are ok
    // But not /inspect/rpc_blabla
    if (StartsWith(path, prefix)) {
      if (path.size() == prefix.size()) {
        return &*h;
      }
      FLARE_CHECK_GT(path.size(), prefix.size());
      char ch = path[prefix.size()];
      if (ch == '/' || ch == '?' || ch == '#') {
        return &*h;
      }
    }
  }
  for (auto&& [regex, h] : regex_paths_) {
    std::cmatch m;
    if (std::regex_match(path.begin(), path.end(), regex)) {
      return &*h;
    }
  }
  return nullptr;
}

HttpFilter::Action Service::RunFilters(HttpRequest* request,
                                       HttpResponse* response,
                                       HttpServerContext* context) {
  for (auto&& e : filters_) {
    auto action = e->OnFilter(request, response, context);
    if (action != HttpFilter::Action::KeepProcessing) {
      return action;
    }
  }
  return HttpFilter::Action::KeepProcessing;
}

void Service::RunHandler(const HttpRequest& request, HttpResponse* response,
                         HttpServerContext* context) {
  auto handler = FindHandler(request.uri());
  if (!handler) {
    default_handler_->HandleRequest(request, response, context);
  } else {
    (*handler).HandleRequest(request, response, context);
  }
}

}  // namespace flare::http
