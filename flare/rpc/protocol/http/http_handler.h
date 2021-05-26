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

#ifndef FLARE_RPC_PROTOCOL_HTTP_HTTP_HANDLER_H_
#define FLARE_RPC_PROTOCOL_HTTP_HTTP_HANDLER_H_

#ifndef FLARE_HTTP_HANDLER_SUPPRESS_INCLUDE_WARNING
#warning Use `flare/rpc/http_handler.h` instead.
#endif

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "flare/base/function.h"
#include "flare/base/net/endpoint.h"
#include "flare/init/on_init.h"
#include "flare/net/http/http_request.h"
#include "flare/net/http/http_response.h"
#include "flare/rpc/protocol/http/http_server_context.h"

namespace flare {

// IMPLEMENTATION DETAILS. YOU SHOULD INCLUDE `flare/rpc/http_handler.h`
// INSTEAD.

// Handler for HTTP requests.
//
// The implementation may return a message whose status code is not 200 should
// an error occurred.
class HttpHandler {
 public:
  virtual ~HttpHandler() = default;

  // NOTICE: You may either override `HandleRequest` or `OnXxx`, but not both.
  // The default implementation of `HandleRequest` is responsible for calling
  // `OnXxx`.

  // If you want to support multiple methods with the same implementation (which
  // is unlikely), you may choose to override this method instead of overriding
  // individual `OnXxx`s.
  //
  // The default implementation forwards calls to `OnXxx`.
  virtual void HandleRequest(const HttpRequest& request, HttpResponse* response,
                             HttpServerContext* context /* Or non-const? */);

  // In most cases you only need to override the method you want to support.
  //
  // The default implementation returns HTTP 405 Method Not Allowed.
  virtual void OnGet(const HttpRequest& request, HttpResponse* response,
                     HttpServerContext* context);
  virtual void OnHead(const HttpRequest& request, HttpResponse* response,
                      HttpServerContext* context);
  virtual void OnPost(const HttpRequest& request, HttpResponse* response,
                      HttpServerContext* context);
  virtual void OnPut(const HttpRequest& request, HttpResponse* response,
                     HttpServerContext* context);
  virtual void OnDelete(const HttpRequest& request, HttpResponse* response,
                        HttpServerContext* context);
  virtual void OnConnect(const HttpRequest& request, HttpResponse* response,
                         HttpServerContext* context);
  virtual void OnOptions(const HttpRequest& request, HttpResponse* response,
                         HttpServerContext* context);
  virtual void OnTrace(const HttpRequest& request, HttpResponse* response,
                       HttpServerContext* context);
  virtual void OnPatch(const HttpRequest& request, HttpResponse* response,
                       HttpServerContext* context);
};

class Server;

namespace detail {

class FunctorHttpHandlerImpl : public HttpHandler {
 public:
  using Impl =
      Function<void(const HttpRequest&, HttpResponse*, HttpServerContext*)>;
  explicit FunctorHttpHandlerImpl(Impl impl);

 protected:
  void Forward(const HttpRequest& request, HttpResponse* response,
               HttpServerContext* context);

 private:
  Impl impl_;
};

void RegisterBuiltinHttpHandlerFactory(
    Function<std::unique_ptr<HttpHandler>(Server*)> f,
    const std::vector<std::string>& path);

const std::vector<std::pair<Function<std::unique_ptr<HttpHandler>(Server*)>,
                            std::vector<std::string>>>&
GetBuiltinHttpHandlers();

void RegisterBuiltinHttpPrefixHandlerFactory(
    Function<std::unique_ptr<HttpHandler>(Server*)> f,
    const std::string& prefix);

const std::vector<
    std::pair<Function<std::unique_ptr<HttpHandler>(Server*)>, std::string>>&
GetBuiltinHttpPrefixHandlers();

}  // namespace detail

// Generates method `New##Name##Handler` (`Name` can be empty), which accepts a
// `Callable` `f`, and returns a `std::unique_ptr<HttpHandler>` to an object
// which forward `Method` to `f`.
#define FLARE_RPC_PROTOCOL_HTTP_DETAIL_DEFINE_FUNCTOR_HANDLER(Name, Method) \
  template <class F>                                                        \
  std::unique_ptr<HttpHandler> NewHttp##Name##Handler(F&& f) {              \
    class Impl : public flare::detail::FunctorHttpHandlerImpl {             \
      using detail::FunctorHttpHandlerImpl::FunctorHttpHandlerImpl;         \
      void Method(const HttpRequest& request, HttpResponse* response,       \
                  HttpServerContext* context) override {                    \
        Forward(request, response, context);                                \
      }                                                                     \
    };                                                                      \
    return std::make_unique<Impl>(std::forward<F>(f));                      \
  }

// These helper classes adapt functor to `HttpHandler`. This can be handy if the
// implementation is simple enough.
//
// Usage:
//
// // Create a new handler accepting `GET` method only.
// std::unique_ptr<HttpHandler> handler = NewHttpGetHandler(
//     [] (auto&& reader, auto&& writer, auto&& context) { /* code here */ });
FLARE_RPC_PROTOCOL_HTTP_DETAIL_DEFINE_FUNCTOR_HANDLER(Get, OnGet)
FLARE_RPC_PROTOCOL_HTTP_DETAIL_DEFINE_FUNCTOR_HANDLER(Head, OnHead)
FLARE_RPC_PROTOCOL_HTTP_DETAIL_DEFINE_FUNCTOR_HANDLER(Post, OnPost)
FLARE_RPC_PROTOCOL_HTTP_DETAIL_DEFINE_FUNCTOR_HANDLER(Put, OnPut)
FLARE_RPC_PROTOCOL_HTTP_DETAIL_DEFINE_FUNCTOR_HANDLER(Delete, OnDelete)
FLARE_RPC_PROTOCOL_HTTP_DETAIL_DEFINE_FUNCTOR_HANDLER(Connect, OnConnect)
FLARE_RPC_PROTOCOL_HTTP_DETAIL_DEFINE_FUNCTOR_HANDLER(Options, OnOptions)
FLARE_RPC_PROTOCOL_HTTP_DETAIL_DEFINE_FUNCTOR_HANDLER(Trace, OnTrace)
FLARE_RPC_PROTOCOL_HTTP_DETAIL_DEFINE_FUNCTOR_HANDLER(Patch, OnPatch)

// FOR INTERNAL USE ONLY.
FLARE_RPC_PROTOCOL_HTTP_DETAIL_DEFINE_FUNCTOR_HANDLER(, HandleRequest)

#undef FLARE_RPC_PROTOCOL_HTTP_DETAIL_DEFINE_FUNCTOR_HANDLER

// Register handler, you need to link the corresponding handler.
#define FLARE_RPC_SERVER_REGISTER_BUILTIN_HTTP_HANDLER(Type, ...)   \
  FLARE_ON_INIT(0, [] {                                             \
    ::flare::detail::RegisterBuiltinHttpHandlerFactory(             \
        [](auto&& owner) { return std::make_unique<Type>(owner); }, \
        {__VA_ARGS__});                                             \
  })

#define FLARE_RPC_SERVER_REGISTER_BUILTIN_HTTP_PREFIX_HANDLER(Func, Prefix) \
  FLARE_ON_INIT(0, [] {                                                     \
    ::flare::detail::RegisterBuiltinHttpPrefixHandlerFactory(Func, Prefix); \
  })

}  // namespace flare

#endif  // FLARE_RPC_PROTOCOL_HTTP_HTTP_HANDLER_H_
