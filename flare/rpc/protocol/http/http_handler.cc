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

#define FLARE_HTTP_HANDLER_SUPPRESS_INCLUDE_WARNING

#include "flare/rpc/protocol/http/http_handler.h"

#include <unordered_map>
#include <utility>

#include "flare/base/internal/lazy_init.h"

namespace flare {

namespace detail {
namespace {

std::vector<std::pair<Function<std::unique_ptr<HttpHandler>(Server*)>,
                      std::vector<std::string>>>*
BuiltinHttpHandlers() {
  // Handler -> Paths
  return internal::LazyInit<
      std::vector<std::pair<Function<std::unique_ptr<HttpHandler>(Server*)>,
                            std::vector<std::string>>>>();
}

std::vector<
    std::pair<Function<std::unique_ptr<HttpHandler>(Server*)>, std::string>>*
BuiltinHttpPrefixHandlers() {
  // Handler -> Prefix
  return internal::LazyInit<std::vector<std::pair<
      Function<std::unique_ptr<HttpHandler>(Server*)>, std::string>>>();
}

}  // namespace

void RegisterBuiltinHttpHandlerFactory(
    Function<std::unique_ptr<HttpHandler>(Server*)> f,
    const std::vector<std::string>& path) {
  BuiltinHttpHandlers()->emplace_back(std::move(f), path);
}

const std::vector<std::pair<Function<std::unique_ptr<HttpHandler>(Server*)>,
                            std::vector<std::string>>>&
GetBuiltinHttpHandlers() {
  return *BuiltinHttpHandlers();
}

void RegisterBuiltinHttpPrefixHandlerFactory(
    Function<std::unique_ptr<HttpHandler>(Server*)> f,
    const std::string& prefix) {
  BuiltinHttpPrefixHandlers()->emplace_back(std::move(f), prefix);
}

const std::vector<
    std::pair<Function<std::unique_ptr<HttpHandler>(Server*)>, std::string>>&
GetBuiltinHttpPrefixHandlers() {
  return *BuiltinHttpPrefixHandlers();
}

FunctorHttpHandlerImpl::FunctorHttpHandlerImpl(Impl impl)
    : impl_(std::move(impl)) {}

void FunctorHttpHandlerImpl::Forward(const HttpRequest& request,
                                     HttpResponse* response,
                                     HttpServerContext* context) {
  return impl_(request, response, context);
}

}  // namespace detail

void HttpHandler::HandleRequest(const HttpRequest& request,
                                HttpResponse* response,
                                HttpServerContext* context) {
  using H = decltype(&HttpHandler::OnGet);

  // FIXME: Array lookup could be faster, but C++'s designated initializes do
  // not support array initialization yet.
  //
  // For the moment I don't think there would be efficiency requirement anyway.
  static const std::unordered_map<HttpMethod, H> kHandlers = {
      {HttpMethod::Get, &HttpHandler::OnGet},
      {HttpMethod::Head, &HttpHandler::OnHead},
      {HttpMethod::Post, &HttpHandler::OnPost},
      {HttpMethod::Put, &HttpHandler::OnPut},
      {HttpMethod::Delete, &HttpHandler::OnDelete},
      {HttpMethod::Connect, &HttpHandler::OnConnect},
      {HttpMethod::Options, &HttpHandler::OnOptions},
      {HttpMethod::Trace, &HttpHandler::OnTrace},
      {HttpMethod::Patch, &HttpHandler::OnPatch},
  };

  auto iter = kHandlers.find(request.method());
  if (iter != kHandlers.end()) {
    (this->*(iter->second))(request, response, context);
  } else {
    GenerateDefaultResponsePage(HttpStatus::MethodNotAllowed, response);
  }
}

void HttpHandler::OnGet(const HttpRequest& request, HttpResponse* response,
                        HttpServerContext* context) {
  GenerateDefaultResponsePage(HttpStatus::MethodNotAllowed, response);
}

void HttpHandler::OnHead(const HttpRequest& request, HttpResponse* response,
                         HttpServerContext* context) {
  GenerateDefaultResponsePage(HttpStatus::MethodNotAllowed, response);
}

void HttpHandler::OnPost(const HttpRequest& request, HttpResponse* response,
                         HttpServerContext* context) {
  GenerateDefaultResponsePage(HttpStatus::MethodNotAllowed, response);
}

void HttpHandler::OnPut(const HttpRequest& request, HttpResponse* response,
                        HttpServerContext* context) {
  GenerateDefaultResponsePage(HttpStatus::MethodNotAllowed, response);
}

void HttpHandler::OnDelete(const HttpRequest& request, HttpResponse* response,
                           HttpServerContext* context) {
  GenerateDefaultResponsePage(HttpStatus::MethodNotAllowed, response);
}

void HttpHandler::OnConnect(const HttpRequest& request, HttpResponse* response,
                            HttpServerContext* context) {
  GenerateDefaultResponsePage(HttpStatus::MethodNotAllowed, response);
}

void HttpHandler::OnOptions(const HttpRequest& request, HttpResponse* response,
                            HttpServerContext* context) {
  GenerateDefaultResponsePage(HttpStatus::MethodNotAllowed, response);
}

void HttpHandler::OnTrace(const HttpRequest& request, HttpResponse* response,
                          HttpServerContext* context) {
  GenerateDefaultResponsePage(HttpStatus::MethodNotAllowed, response);
}

void HttpHandler::OnPatch(const HttpRequest& request, HttpResponse* response,
                          HttpServerContext* context) {
  GenerateDefaultResponsePage(HttpStatus::MethodNotAllowed, response);
}

}  // namespace flare
