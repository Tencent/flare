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

#ifndef FLARE_RPC_PROTOCOL_HTTP_MESSAGE_H_
#define FLARE_RPC_PROTOCOL_HTTP_MESSAGE_H_

#include <string>
#include <string_view>
#include <utility>

#include "flare/base/buffer.h"
#include "flare/net/http/http_request.h"
#include "flare/net/http/http_response.h"
#include "flare/rpc/protocol/message.h"

namespace flare::http {

constexpr std::string_view kAcceptEncoding = "Accept-Encoding";
constexpr std::string_view kContentLength = "Content-Length";
constexpr std::string_view kContentEncoding = "Content-Encoding";
constexpr std::string_view kTransferEncoding = "Transfer-Encoding";
constexpr std::string_view kTrailer = "Trailer";
constexpr std::string_view kConnection = "Connection";

class Http11ClientSideProtocol;
class Http11ServerSideProtocol;

// Common HttpRequest/Response operations.
class HttpBaseMessage : public Message {
 public:
  virtual void SetBody(NoncontiguousBuffer&& body) = 0;
  virtual void SetHeaders(HttpHeaders&& headers) = 0;
  virtual HttpHeaders* headers() = 0;
  virtual NoncontiguousBuffer* noncontiguous_body() = 0;
  virtual std::string* body() = 0;

 private:
  std::string trailer_;
};

// Adaptor for HttpRequest
class HttpRequestMessage : public HttpBaseMessage {
  friend class Http11ServerSideProtocol;

 public:
  HttpRequestMessage() { SetRuntimeTypeTo<HttpRequestMessage>(); }
  explicit HttpRequestMessage(HttpRequest http_request);

  std::uint64_t GetCorrelationId() const noexcept override;
  Type GetType() const noexcept override;

  HttpHeaders* headers() override { return request()->headers(); }
  NoncontiguousBuffer* noncontiguous_body() override {
    return request()->noncontiguous_body();
  }
  std::string* body() override { return request()->body(); }

  HttpRequest* request() { return &http_request_; }
  const HttpRequest* request() const { return &http_request_; }

  void SetBody(NoncontiguousBuffer&& body) override {
    request()->set_body(std::move(body));
  }
  void SetHeaders(HttpHeaders&& headers) override {
    *request()->headers() = std::move(headers);
  }

 private:
  HttpRequest http_request_;
};

// Adaptor for HttpResponse
class HttpResponseMessage : public HttpBaseMessage {
  friend class Http11ClientSideProtocol;

 public:
  HttpResponseMessage() { SetRuntimeTypeTo<HttpResponseMessage>(); }
  explicit HttpResponseMessage(HttpResponse http_response);

  std::uint64_t GetCorrelationId() const noexcept override;
  Type GetType() const noexcept override;

  HttpHeaders* headers() override { return response()->headers(); }
  NoncontiguousBuffer* noncontiguous_body() override {
    return response()->noncontiguous_body();
  }
  std::string* body() override { return response()->body(); }

  HttpResponse* response() { return &http_response_; }
  const HttpResponse* response() const { return &http_response_; }

  void SetBody(NoncontiguousBuffer&& body) override {
    response()->set_body(std::move(body));
  }
  void SetHeaders(HttpHeaders&& headers) override {
    *response()->headers() = std::move(headers);
  }

 private:
  HttpResponse http_response_;
};

}  // namespace flare::http

namespace flare {

// HACK: Allows `CastingTraits<HttpBaseMessage>` to recognize its subclasses.
template <>
struct CastingTraits<http::HttpBaseMessage> {
  static bool RuntimeTypeCheck(const Message& m) {
    return GetRuntimeType(m) ==
               Castable::kRuntimeType<http::HttpRequestMessage> ||
           GetRuntimeType(m) ==
               Castable::kRuntimeType<http::HttpResponseMessage>;
  }
};

}  // namespace flare

#endif  // FLARE_RPC_PROTOCOL_HTTP_MESSAGE_H_
