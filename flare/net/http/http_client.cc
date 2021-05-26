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

#include "flare/net/http/http_client.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>

#include "thirdparty/curl/curl.h"
#include "thirdparty/gflags/gflags.h"

#include "flare/fiber/latch.h"
#include "flare/fiber/runtime.h"
#include "flare/net/http/packet_desc.h"
#include "flare/net/internal/http_engine.h"
#include "flare/rpc/internal/session_context.h"
#include "flare/rpc/message_dispatcher/message_dispatcher.h"
#include "flare/rpc/protocol/http/binlog.pb.h"

using namespace std::literals;

DEFINE_int32(flare_http_client_default_timeout_ms, 1000,
             "Default timeout of flare::HttpClient.");

namespace flare {

namespace {

HttpClient::ErrorCode GetErrorCodeFromCurlCode(int c) {
  switch (c) {
    case CURLE_UNSUPPORTED_PROTOCOL:
      return HttpClient::ERROR_PROTOCOL_NOT_SUPPORTED;
    case CURLE_URL_MALFORMAT:
      return HttpClient::ERROR_INVALID_URI_ADDRESS;
    case CURLE_COULDNT_RESOLVE_PROXY:
      return HttpClient::ERROR_PROXY;
    case CURLE_COULDNT_RESOLVE_HOST:
      return HttpClient::ERROR_FAIL_TO_RESOLVE_ADDRESS;
    case CURLE_COULDNT_CONNECT:
      return HttpClient::ERROR_CONNECTION;
    case CURLE_HTTP2:
    case CURLE_HTTP2_STREAM:
      return HttpClient::ERROR_HTTP2;
    case CURLE_HTTP_RETURNED_ERROR:
    case CURLE_HTTP_POST_ERROR:
      FLARE_LOG_WARNING_EVERY_SECOND("ERROR_CURL_HTTP_ERROR CURLcode {}", c);
      return HttpClient::ERROR_INTERNAL_ERROR;
    case CURLE_OPERATION_TIMEDOUT:
      return HttpClient::ERROR_TIMEOUT;
    case CURLE_SSL_CONNECT_ERROR:
    case CURLE_SSL_ENGINE_NOTFOUND:
    case CURLE_SSL_ENGINE_SETFAILED:
    case CURLE_SSL_CERTPROBLEM:
    case CURLE_SSL_CIPHER:
    case CURLE_PEER_FAILED_VERIFICATION:
      FLARE_LOG_WARNING_EVERY_SECOND("ERROR_SSL CURLcode {}", c);
      return HttpClient::ERROR_SSL;
    case CURLE_SEND_ERROR:
    case CURLE_RECV_ERROR:
      return HttpClient::ERROR_IO;
    case CURLE_TOO_MANY_REDIRECTS:
      return HttpClient::ERROR_TOO_MANY_REDIRECTS;
    default:
      FLARE_LOG_WARNING_EVERY_SECOND("ERROR_UNKNOWN CURLcode {}", c);
      return HttpClient::ERROR_UNKNOWN;
  }
}

long FlareHttpVersionToCurlHttpVersion(HttpVersion v,
                                       bool no_automatic_upgrade) {
  if (no_automatic_upgrade) {
    return CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE;
  }
  switch (v) {
    case HttpVersion::Unspecified:
      return CURL_HTTP_VERSION_NONE;
    case HttpVersion::V_1_0:
      return CURL_HTTP_VERSION_1_0;
    case HttpVersion::V_1_1:
      return CURL_HTTP_VERSION_1_1;
    case HttpVersion::V_2:
      return CURL_HTTP_VERSION_2_0;
    case HttpVersion::V_3:
      return CURL_HTTP_VERSION_3;
    default:
      FLARE_CHECK(0, "Unknown flare http version");
  }
}

HttpVersion CurlHttpVersionToHttpVersion(long v) {
  switch (v) {
    case CURL_HTTP_VERSION_NONE:
      return HttpVersion::Unspecified;
    case CURL_HTTP_VERSION_1_0:
      return HttpVersion::V_1_0;
    case CURL_HTTP_VERSION_1_1:
      return HttpVersion::V_1_1;
    case CURL_HTTP_VERSION_2_0:
      return HttpVersion::V_2;
    case CURL_HTTP_VERSION_3:
      return HttpVersion::V_3;
    default:
      FLARE_CHECK(0, "Unknown flare http version {}", v);
  }
}

void FillResponseInfo(CURL* easy_handler,
                      HttpClient::ResponseInfo* response_info) {
  FLARE_CHECK(response_info);
  char* effective_url = nullptr;
  curl_easy_getinfo(easy_handler, CURLINFO_EFFECTIVE_URL, &effective_url);
  response_info->effective_url.assign(effective_url);

  double transfer_time_in_seconds;
  curl_easy_getinfo(easy_handler, CURLINFO_TOTAL_TIME,
                    &transfer_time_in_seconds);
  response_info->total_time_transfer =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::duration<double>(transfer_time_in_seconds));

  long http_version;
  curl_easy_getinfo(easy_handler, CURLINFO_HTTP_VERSION, &http_version);
  response_info->http_version = CurlHttpVersionToHttpVersion(http_version);
}

std::optional<std::pair<std::string, Function<void(bool)>>> OverrideHost(
    const std::string& url, const std::string& override_host_nslb) {
  if (override_host_nslb.empty()) {
    return std::pair<std::string, Function<void(bool)>>{url, nullptr};
  }
  thread_local std::map<std::string, std::unique_ptr<MessageDispatcher>>
      loadbalancer_map;
  auto start_pos = url.find("://");
  if (start_pos == std::string::npos) {
    return std::nullopt;
  }
  start_pos += 3;
  auto end_pos = url.find("/", start_pos);
  if (end_pos == std::string::npos) {
    return std::nullopt;
  }
  auto host = url.substr(start_pos, end_pos - start_pos);

  auto it = loadbalancer_map.find(host);
  if (it == loadbalancer_map.end()) {
    auto dis = message_dispatcher_registry.TryNew(override_host_nslb);
    if (!dis) {
      return std::nullopt;
    }
    if (!dis->Open(host)) {
      return std::nullopt;
    }
    auto [it1, _] = loadbalancer_map.insert({host, std::move(dis)});
    it = it1;
  }

  auto message_dispatcher = it->second.get();
  Endpoint ep;
  std::uintptr_t message_dispatcher_ctx;
  if (!message_dispatcher->GetPeer(0, &ep, &message_dispatcher_ctx)) {
    return std::nullopt;
  }

  // We are in fiber environment, message_dispatcher will not destroy
  auto report_function = [start = ReadSteadyClock(), message_dispatcher, ep,
                          message_dispatcher_ctx](bool succ) {
    auto status =
        succ ? LoadBalancer::Status::Success : LoadBalancer::Status::Failed;
    message_dispatcher->Report(ep, status, ReadSteadyClock() - start,
                               message_dispatcher_ctx);
  };
  return std::pair<std::string, Function<void(bool)>>{
      url.substr(0, start_pos) + ep.ToString() + url.substr(end_pos),
      report_function};
}

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

class HttpEngineWrapper : public detail::HttpChannel {
 public:
  static HttpEngineWrapper* Instance() {
    static NeverDestroyedSingleton<HttpEngineWrapper> engine;
    return engine.Get();
  }

  internal::HttpTask GetHttpTask(
      const std::string& url, const HttpClient::Options opts,
      const HttpClient::RequestOptions& request_options) {
    internal::HttpTask task;
    task.SetUrl(url);
    task.SetTimeout(request_options.timeout);
    CURL* h = task.GetNativeHandle();
    if (opts.follow_redirects) {
      FLARE_CHECK_EQ(curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L), CURLE_OK);
      FLARE_CHECK_EQ(curl_easy_setopt(h, CURLOPT_MAXREDIRS,
                                      request_options.max_redirection_count),
                     CURLE_OK);
    }
    if (request_options.verbose) {
      FLARE_CHECK_EQ(curl_easy_setopt(h, CURLOPT_VERBOSE, 1L), CURLE_OK);
    }
    if (opts.use_builtin_compression) {
      // libcurl only supports identity, gzip, br, deflate now.
      // And our libcurl is compiled without br.
      FLARE_CHECK_EQ(curl_easy_setopt(h, CURLOPT_ACCEPT_ENCODING,
                                      "identity, gzip, deflate"),
                     CURLE_OK);
    }
    FLARE_CHECK_EQ(curl_easy_setopt(h, CURLOPT_HTTP_VERSION,
                                    FlareHttpVersionToCurlHttpVersion(
                                        request_options.http_version,
                                        request_options.no_automatic_upgrade)),
                   CURLE_OK);
    for (auto&& s : request_options.headers) {
      task.AddHeader(s);
    }
    if (!request_options.content_type.empty()) {
      task.AddHeader("Content-Type: " + request_options.content_type);
    }
    if (!opts.verify_server_certificate) {
      FLARE_CHECK_EQ(curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 0), CURLE_OK);
      FLARE_CHECK_EQ(curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 0), CURLE_OK);
    }
    // Default use env proxy.
    if (!opts.proxy_from_env) {
      // Empty will explicitly disable the use of a proxy
      FLARE_CHECK_EQ(curl_easy_setopt(h, CURLOPT_PROXY, opts.proxy.c_str()),
                     CURLE_OK);
    }

    return task;
  }

  std::string WriteBinlogContext(const HttpResponse& response) {
    http::SerializedClientPacket serialized;
    serialized.set_status(static_cast<uint32_t>(response.status()));
    serialized.set_version(static_cast<uint32_t>(response.version()));
    serialized.set_body(*response.body());
    for (auto&& [k, v] : *response.headers()) {
      auto p = serialized.add_headers();
      p->set_key(std::string(k));
      p->set_value(std::string(v));
    }
    return serialized.SerializeAsString();
  }

  void AsyncCallCallback(
      RefPtr<fiber::ExecutionContext> ec,
      Function<void(Expected<HttpResponse, HttpClient::ErrorCode>)> done,
      Function<void(bool)> report_function,
      Expected<internal::HttpTaskCompletion, Status> completion,
      HttpClient::ResponseInfo* response_info,
      binlog::OutgoingCallWriter* binlog_writer) {
    fiber::WithExecutionContextIfPresent(ec.Get(), [&] {
      if (report_function) {
        report_function(!!completion);
      }
      std::optional<HttpResponse> opt_response;
      if (completion) {
        HttpResponse response;
        response.set_version(completion->version());
        response.set_status(completion->status());
        response.set_body(std::move(*completion->body()));
        CopyHeaders(*completion->headers(), response.headers());
        opt_response = std::move(response);
      }

      if (completion && response_info) {
        FillResponseInfo(completion->GetNativeHandle(), response_info);
      }
      if (FLARE_UNLIKELY(binlog_writer)) {
        binlog_writer->SetFinishTimestamp(flare::ReadSteadyClock());
        if (completion) {
          binlog_writer->SetInvocationStatus("0");
          binlog_writer->AddIncomingPacket(http::PacketDesc(*opt_response),
                                           WriteBinlogContext(*opt_response));
        } else {
          binlog_writer->SetInvocationStatus(
              std::to_string(completion.error().code()));
        }
      }
      if (!completion) {
        done(GetErrorCodeFromCurlCode(completion.error().code()));
      } else {
        FLARE_CHECK(!!opt_response);
        done(std::move(*opt_response));
      }
    });
  }

  template <class F>
  void AsyncCall(
      HttpMethod method, const std::string& url,
      const HttpClient::Options& opts,
      const HttpClient::RequestOptions& request_options,
      HttpClient::ResponseInfo* response_info,
      Function<void(Expected<HttpResponse, HttpClient::ErrorCode>)>&& done,
      F&& callback) {
    auto&& override_host =
        OverrideHost(url, request_options.override_host_nslb);
    if (!override_host) {
      done(HttpClient::ERROR_FAIL_TO_RESOLVE_ADDRESS);
      return;
    }
    auto&& [effective_url, report_function] = *override_host;
    auto task = GetHttpTask(effective_url, opts, request_options);
    task.SetMethod(method);
    std::unique_ptr<HttpRequest> request;
    binlog::OutgoingCallWriter* binlog_writer = nullptr;
    if (rpc::IsBinlogDumpContextPresent()) {
      binlog_writer = rpc::session_context->binlog.dumper->StartOutgoingCall();
      binlog_writer->SetCorrelationId(detail::GetHttpBinlogCorrelationId(
          url, request_options.binlog_correlation_id));
      binlog_writer->SetStartTimestamp(flare::ReadSteadyClock());
      binlog_writer->SetUri(url);
      request = std::make_unique<HttpRequest>();
      request->set_version(request_options.http_version);
      for (auto&& header : request_options.headers) {
        auto pos = header.find(":");
        if (pos != std::string::npos) {
          request->headers()->Append(header.substr(0, pos),
                                     std::string(Trim(header.substr(pos + 1))));
        }
      }
    }
    callback(&task, request.get());
    if (binlog_writer) {
      binlog_writer->AddOutgoingPacket(http::PacketDesc(*request));
    }
    internal::HttpEngine::Instance()->StartTask(
        std::move(task),
        [ec = rpc::CaptureSessionContext(), done = std::move(done),
         report_function = std::move(report_function), response_info,
         binlog_writer, this](auto&& completion) mutable {
          AsyncCallCallback(ec, std::move(done), std::move(report_function),
                            std::move(completion), response_info,
                            binlog_writer);
        });
  }

  void AsyncGet(const std::string& url, const HttpClient::Options& opts,
                const HttpClient::RequestOptions& request_options,
                HttpClient::ResponseInfo* response_info,
                Function<void(Expected<HttpResponse, HttpClient::ErrorCode>)>&&
                    done) override {
    auto cb = [url](auto&&, auto&& p_request) {
      if (p_request) {
        p_request->set_method(HttpMethod::Get);
        if (auto uri = GetHttpRequestUriFromUrl(url); uri) {
          p_request->set_uri(*uri);
        }
      }
    };
    return AsyncCall(HttpMethod::Get, url, opts, request_options, response_info,
                     std::move(done), std::move(cb));
  }

  void AsyncPost(const std::string& url, const HttpClient::Options& opts,
                 std::string data,
                 const HttpClient::RequestOptions& request_options,
                 HttpClient::ResponseInfo* response_info,
                 Function<void(Expected<HttpResponse, HttpClient::ErrorCode>)>&&
                     done) override {
    auto cb = [url, data = std::move(data)](auto&& p_task, auto&& p_request) {
      if (p_request) {
        p_request->set_method(HttpMethod::Post);
        if (auto uri = GetHttpRequestUriFromUrl(url); uri) {
          p_request->set_uri(*uri);
        }
        p_request->set_body(data);
      }
      p_task->SetBody(std::move(data));
    };
    return AsyncCall(HttpMethod::Post, url, opts, request_options,
                     response_info, std::move(done), std::move(cb));
  }

  void AsyncRequest(
      const std::string& protocol, const std::string& host,
      const HttpClient::Options& opts, const HttpRequest& request,
      const HttpClient::RequestOptions& request_options,
      HttpClient::ResponseInfo* response_info,
      Function<void(Expected<HttpResponse, HttpClient::ErrorCode>)>&& done)
      override {
    auto url = protocol + "://" + host + request.uri();
    auto cb = [request](auto&& p_task, auto&& p_request) {
      if (p_request) {
        *p_request = request;
      }
      if (request.body_size() > 0) {
        if (request.noncontiguous_body()) {
          p_task->SetBody(*request.noncontiguous_body());
        } else {
          p_task->SetBody(*request.body());
        }
      }
      for (auto&& [k, v] : *request.headers()) {
        p_task->AddHeader(std::string(k) + ": " + std::string(v));
      }
      FLARE_CHECK(request.method() != HttpMethod::Unspecified,
                  "You should specify http method!");
      FLARE_CHECK_EQ(
          curl_easy_setopt(p_task->GetNativeHandle(), CURLOPT_CUSTOMREQUEST,
                           ToStringView(request.method()).data()),
          CURLE_OK);
    };
    return AsyncCall(request.method(), url, opts, request_options,
                     response_info, std::move(done), std::move(cb));
  }

 private:
  void CopyHeaders(const std::vector<std::string>& from, HttpHeaders* to) {
    for (auto&& s : from) {
      if (auto pos = s.find(":"); pos != std::string::npos) {
        auto name = s.substr(0, pos);
        auto value = s.substr(pos + 1);
        FLARE_CHECK(!EndsWith(value, "\r\n"));
        to->Append(std::string(name), std::string(Trim(value)));
      }
    }
  }
};

detail::HttpChannel* mock_channel = nullptr;
detail::HttpChannel* dry_run_channel = nullptr;

bool IsMockAddress(const std::string& url_or_protocol, bool is_url) {
  if (is_url) {
    return StartsWith(url_or_protocol, "mock://");
  }
  return url_or_protocol == "mock";
}

detail::HttpChannel* GetHttpChannel(const std::string& url_or_protocol,
                                    bool is_url = true) {
  if (FLARE_UNLIKELY(IsMockAddress(url_or_protocol, is_url))) {
    FLARE_CHECK(mock_channel,
                "Mock channel has not been registered yet. Did you forget to "
                "link `flare/testing:http_mock`?");
    return mock_channel;
  }
  if (FLARE_UNLIKELY(rpc::IsDryRunContextPresent())) {
    FLARE_CHECK(dry_run_channel, "Dry run channel should be registed!");
    return dry_run_channel;
  }
  return HttpEngineWrapper::Instance();
}

}  // namespace

namespace detail {

void RegisterMockHttpChannel(HttpChannel* channel) {
  FLARE_CHECK(!mock_channel, "Mock channel has already been registered");
  mock_channel = channel;
}

void RegisterDryRunHttpChannel(HttpChannel* channel) {
  FLARE_CHECK(!dry_run_channel, "Dry run channel has already been registered");
  dry_run_channel = channel;
}

std::string GetHttpBinlogCorrelationId(const std::string& url,
                                       const std::string& correlation_id) {
  return Format("Http-{}-{}-{}", url,
                rpc::session_context->binlog.correlation_id, correlation_id);
}

}  // namespace detail

HttpClient::HttpClient(const Options& options) : options_(options) {}

const char* HttpClient::ErrorCodeToString(int error_code) {
  switch (static_cast<ErrorCode>(error_code)) {
    case ERROR_INVALID:
      return "Invalid";
    case ERROR_INVALID_URI_ADDRESS:
      return "Invalid URI address";
    case ERROR_FAIL_TO_RESOLVE_ADDRESS:
      return "Failed to resolve address";
    case ERROR_FAIL_TO_SEND_REQUEST:
      return "Failed to send request";
    case ERROR_FAIL_TO_GET_RESPONSE:
      return "Failed to get response";
    case ERROR_CONNECTION:
      return "Connection io error";
    case ERROR_TIMEOUT:
      return "Response timeout";
    case ERROR_PROXY:
      return "ERROR_PROXY";
    case ERROR_PARSE_RESPONSE:
      return "Failed to parse response";
    case ERROR_FAIL_TO_CONNECT_SERVER:
      return "Failed to connect to server";
    case ERROR_PROTOCOL_NOT_SUPPORTED:
      return "Protocol is not supported";
    case ERROR_TOO_MANY_REDIRECTS:
      return "Too many redirections";
    case ERROR_REDIRECT_LOCATION_NOT_FOUND:
      return "redict location not found";
    case ERROR_DECOMPRESS_RESPONSE:
      return "Failed to decompress response";
    case ERROR_HTTP2:
      return "ERROR_HTTP2";
    case ERROR_SSL:
      return "ERROR_SSL";
    case ERROR_IO:
      return "ERROR_IO";
    case ERROR_INTERNAL_ERROR:
      return "ERROR_INTERNAL_ERROR";
    case ERROR_DRY_RUN:
      return "ERROR_DRY_RUN";
    case ERROR_UNKNOWN:
      return "<Unknown>";
  }
  return "<Unknown>";
}

Expected<HttpResponse, HttpClient::ErrorCode> HttpClient::Get(
    const std::string& url, const RequestOptions& request_options,
    ResponseInfo* response_info) {
  fiber::Latch l(1);
  Expected<HttpResponse, HttpClient::ErrorCode> e;
  GetHttpChannel(url)->AsyncGet(url, options_, request_options, response_info,
                                [&](auto&& res) mutable {
                                  e = std::move(res);
                                  l.count_down();
                                });
  l.wait();
  return e;
}

Expected<HttpResponse, HttpClient::ErrorCode> HttpClient::Post(
    const std::string& url, std::string data,
    const RequestOptions& request_options, ResponseInfo* response_info) {
  fiber::Latch l(1);
  Expected<HttpResponse, HttpClient::ErrorCode> e;
  GetHttpChannel(url)->AsyncPost(url, options_, std::move(data),
                                 request_options, response_info,
                                 [&](auto&& res) mutable {
                                   e = std::move(res);
                                   l.count_down();
                                 });
  l.wait();
  return e;
}

Expected<HttpResponse, HttpClient::ErrorCode> HttpClient::Request(
    const std::string& protocol, const std::string& host,
    const HttpRequest& request, const RequestOptions& request_options,
    ResponseInfo* response_info) {
  fiber::Latch l(1);
  Expected<HttpResponse, HttpClient::ErrorCode> e;
  GetHttpChannel(protocol, false)
      ->AsyncRequest(protocol, host, options_, request, request_options,
                     response_info, [&](auto&& res) mutable {
                       e = std::move(res);
                       l.count_down();
                     });
  l.wait();
  return e;
}

Future<Expected<HttpResponse, HttpClient::ErrorCode>> HttpClient::AsyncGet(
    const std::string& url, const RequestOptions& request_options,
    ResponseInfo* response_info) {
  Promise<Expected<HttpResponse, HttpClient::ErrorCode>> p;
  auto future = p.GetFuture();
  GetHttpChannel(url)->AsyncGet(
      url, options_, request_options, response_info,
      [p = std::move(p)](auto&& res) mutable { p.SetValue(res); });
  return future;
}

Future<Expected<HttpResponse, HttpClient::ErrorCode>> HttpClient::AsyncPost(
    const std::string& url, std::string data,
    const RequestOptions& request_options, ResponseInfo* response_info) {
  Promise<Expected<HttpResponse, HttpClient::ErrorCode>> p;
  auto future = p.GetFuture();
  GetHttpChannel(url)->AsyncPost(
      url, options_, std::move(data), request_options, response_info,
      [p = std::move(p)](auto&& res) mutable { p.SetValue(res); });
  return future;
}

Future<Expected<HttpResponse, HttpClient::ErrorCode>> HttpClient::AsyncRequest(
    const std::string& protocol, const std::string& host,
    const HttpRequest& request, const RequestOptions& request_options,
    ResponseInfo* response_info) {
  Promise<Expected<HttpResponse, HttpClient::ErrorCode>> p;
  auto future = p.GetFuture();
  GetHttpChannel(protocol, false)
      ->AsyncRequest(
          protocol, host, options_, request, request_options, response_info,
          [p = std::move(p)](auto&& res) mutable { p.SetValue(res); });
  return future;
}

}  // namespace flare
