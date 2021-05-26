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

#ifndef FLARE_NET_HTTP_HTTP_CLIENT_H_
#define FLARE_NET_HTTP_HTTP_CLIENT_H_

#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "thirdparty/gflags/gflags_declare.h"

#include "flare/base/expected.h"
#include "flare/base/function.h"
#include "flare/base/future.h"
#include "flare/base/internal/lazy_init.h"
#include "flare/net/http/http_request.h"
#include "flare/net/http/http_response.h"

DECLARE_int32(flare_http_client_default_timeout_ms);

namespace flare {

class HttpClient {
 public:
  struct Options {
    bool verify_server_certificate = true;
    bool follow_redirects = true;
    bool proxy_from_env = true;
    // Valid if proxy_from_env is false.
    // Left empty if no proxy.
    std::string proxy;
    // If use_builtin_compression is true, we will add builtin encoding we
    // support (gzip, deflate) in the header Accept-Encoding. And if the
    // server compresses the response, we will do the auto-decompress for you.
    // Attention: in this case, you should not set the Accept-Encoding field in
    // headers yourself.
    //
    // You should keep it false
    // if you don't want to get response with any encoding
    // OR you want to set custom encoding, ex: snappy..
    // OR you want to decompress by yourself
    // In this case, you should set the custom encoding you want in the http
    // header and we will not do the auto-decompress for you, even the
    // compression is gzip or deflate. you should see if the response
    // is compressed(By header Content-Encoding) and decompress it by yourself.
    bool use_builtin_compression = false;
  };

  enum ErrorCode {
    ERROR_INVALID = 0,  // unused
    ERROR_INVALID_URI_ADDRESS,
    ERROR_FAIL_TO_RESOLVE_ADDRESS,
    ERROR_FAIL_TO_SEND_REQUEST,
    ERROR_FAIL_TO_GET_RESPONSE,
    ERROR_CONNECTION,
    ERROR_TIMEOUT,
    ERROR_PARSE_RESPONSE,
    ERROR_FAIL_TO_CONNECT_SERVER,
    ERROR_PROTOCOL_NOT_SUPPORTED,
    ERROR_TOO_MANY_REDIRECTS,
    ERROR_REDIRECT_LOCATION_NOT_FOUND,
    ERROR_DECOMPRESS_RESPONSE,
    ERROR_PROXY,
    ERROR_HTTP2,
    ERROR_SSL,
    ERROR_IO,
    ERROR_INTERNAL_ERROR,
    ERROR_DRY_RUN,
    ERROR_UNKNOWN = 100,
  };

  static const char* ErrorCodeToString(int error_code);

  struct RequestOptions {
    // Set it to -1 for an infinite number of redirects.
    // Setting the limit to 0 will make libcurl refuse any redirect.
    int max_redirection_count = 1;
    std::chrono::nanoseconds timeout =
        std::chrono::milliseconds(FLAGS_flare_http_client_default_timeout_ms);
    // You can either set content_type OR set "Content-Type" field in headers.
    std::string content_type;          // ex: text:html
    std::vector<std::string> headers;  // string ex: "Content-Type: text/html"
    bool verbose = false;              // Use for debug, print info to stderr.

    // http version we want to use.
    bool no_automatic_upgrade = false;  // use http2 without 1.1 upgrade
    // http_version valid if no_automatic_upgrade is false
    HttpVersion http_version = HttpVersion::Unspecified;

    // We extend the host field in the url to support nslb address.
    // If no empty, host will be overridden by the corresponding nslb.
    std::string override_host_nslb;

    // Needed only if you request to the same Http URI more than once in a
    // single RPC session.
    std::string binlog_correlation_id;
  };

  struct ResponseInfo {
    std::string effective_url;                     // get the last used URL.
    std::chrono::nanoseconds total_time_transfer;  // transfer time.
    HttpVersion http_version;                      // http version actual used.
  };

  explicit HttpClient(
      const Options& options = internal::LazyInitConstant<Options>());

  Future<Expected<HttpResponse, ErrorCode>> AsyncGet(
      const std::string& url,
      const RequestOptions& request_options =
          internal::LazyInitConstant<RequestOptions>(),
      ResponseInfo* response_detail = nullptr);  // can be empty
  Expected<HttpResponse, ErrorCode> Get(
      const std::string& url,
      const RequestOptions& request_options =
          internal::LazyInitConstant<RequestOptions>(),
      ResponseInfo* response_info = nullptr);

  Future<Expected<HttpResponse, ErrorCode>> AsyncPost(
      const std::string& url, std::string data,
      const RequestOptions&
          request_options,  // should set content-type OR
                            // "Content-Type" field in headers.
      ResponseInfo* response_info = nullptr);
  Expected<HttpResponse, ErrorCode> Post(const std::string& url,
                                         std::string data,
                                         const RequestOptions& request_options,
                                         ResponseInfo* response_info = nullptr);

  // You can control every details, but must fill the request correctly.
  // protocol: http or https or mock
  // host: domain or ip:port
  Future<Expected<HttpResponse, ErrorCode>> AsyncRequest(
      const std::string& protocol, const std::string& host,
      const HttpRequest& request,
      const RequestOptions& request_options =
          internal::LazyInitConstant<RequestOptions>(),
      ResponseInfo* response_info = nullptr);  // can be empty
  Expected<HttpResponse, ErrorCode> Request(
      const std::string& protocol, const std::string& host,
      const HttpRequest& request,
      const RequestOptions& request_options =
          internal::LazyInitConstant<RequestOptions>(),
      ResponseInfo* response_info = nullptr);

 private:
  Options options_;
};

namespace detail {

// For internal use. Do NOT inherit this class.
class HttpChannel {
 public:
  HttpChannel() = default;
  virtual ~HttpChannel() = default;

  virtual void AsyncGet(
      const std::string& url, const HttpClient::Options& opts,
      const HttpClient::RequestOptions& request_options,
      HttpClient::ResponseInfo* response_info,
      Function<void(Expected<HttpResponse, HttpClient::ErrorCode>)>&& done) = 0;

  virtual void AsyncPost(
      const std::string& url, const HttpClient::Options& opts, std::string data,
      const HttpClient::RequestOptions& request_options,
      HttpClient::ResponseInfo* response_info,
      Function<void(Expected<HttpResponse, HttpClient::ErrorCode>)>&& done) = 0;

  virtual void AsyncRequest(
      const std::string& protocol, const std::string& host,
      const HttpClient::Options& opts, const HttpRequest& request,
      const HttpClient::RequestOptions& request_options,
      HttpClient::ResponseInfo* response_info,
      Function<void(Expected<HttpResponse, HttpClient::ErrorCode>)>&& done) = 0;
};

// For internal use. Do NOT call this method.
//
// Must be called before entering multi-threaded environment.
void RegisterMockHttpChannel(HttpChannel* channel);
void RegisterDryRunHttpChannel(HttpChannel* channel);

std::string GetHttpBinlogCorrelationId(const std::string& url,
                                       const std::string& correlation_id);

}  // namespace detail

}  // namespace flare

#endif  // FLARE_NET_HTTP_HTTP_CLIENT_H_
