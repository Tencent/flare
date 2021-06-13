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

#ifndef FLARE_TESTING_HTTP_MOCK_H_
#define FLARE_TESTING_HTTP_MOCK_H_

#include <string>
#include <tuple>

#include "gmock/gmock.h"

#include "flare/base/internal/lazy_init.h"
#include "flare/net/http/http_client.h"
#include "flare/testing/detail/gmock_actions.h"

// Usage: `FLARE_EXPECT_HTTP({UrlMatcher}, {MethodMatcher},
// {HeaderMatcher}, {BodyMathcer})s...`
//
// For HeaderMatcher, you can use HeaderContains/HeaderEq.
//
// To manually provide a response (either a successful one or an error), use
// `FLARE_EXPECT_HTTP(...).WillXxx(flare::testing::Return(...))`.
//
// Currently the following are supported:
//
// - `flare::testing::Return(const flare::HttpResponse&)`: Complete the
//   Http with the given response.
//
// - `flare::testing::Return(const flare::HttpResponse&, const
//   flare::HttpClient::ResponseInfo& info)`: Complete the Http with the given
//   response and set the response_info with the given info.
//
// - `flare::testing::Return(flare::HttpClient::ErrorCode)`: Fail the Http with
//   the given error code.
#define FLARE_EXPECT_HTTP(Url, Method, Header, Body)                    \
                                                                        \
  EXPECT_CALL(*::flare::internal::LazyInit<                             \
                  ::flare::testing::detail::HttpMockChannel>(),         \
              MockAsyncRequest(::testing::_, Url, Method, Header, Body, \
                               ::testing::_ /* ResponseInfo*/,          \
                               ::testing::_ /* response, ignored*/))

namespace flare::testing {

// These matcheres for header are only for Request, not for Get or Post.
MATCHER_P(HttpHeaderContains, key, "Http Header contains") {
  return arg.contains(key);
}

MATCHER_P2(HttpHeaderEq, key, val, "Http Header eq") {
  auto&& opt = arg.TryGet(key);
  return opt && std::string(*opt) == val;
}

}  // namespace flare::testing

////////////////////////////////////////
// Implementation goes below.         //
////////////////////////////////////////

namespace flare::testing {

namespace detail {

// Adopted from gdt's rpc mock.
class HttpMockChannel : public flare::detail::HttpChannel {
 public:
  HttpMockChannel();

  // Gmock doesn't support move, so we use pointer for as an intermediate
  // conversion.
  MOCK_METHOD7(
      MockAsyncRequest,
      void(flare::detail::HttpChannel*,
           const std::string&,  // url
           const HttpMethod&,   // method
           const HttpHeaders&,  // headers
           const std::string&,  // body
           HttpClient::ResponseInfo*,
           Function<void(Expected<HttpResponse, HttpClient::ErrorCode>)>*));

  void AsyncGet(const std::string& url, const HttpClient::Options& options,
                const HttpClient::RequestOptions& request_options,
                HttpClient::ResponseInfo* response_info,
                Function<void(Expected<HttpResponse, HttpClient::ErrorCode>)>&&
                    done) override {
    MockAsyncRequest(this, url, HttpMethod::Get, {}, "", response_info, &done);
  }

  void AsyncPost(const std::string& url, const HttpClient::Options& options,
                 std::string data,
                 const HttpClient::RequestOptions& request_options,
                 HttpClient::ResponseInfo* response_info,
                 Function<void(Expected<HttpResponse, HttpClient::ErrorCode>)>&&
                     done) override {
    MockAsyncRequest(this, url, HttpMethod::Post, {}, std::move(data),
                     response_info, &done);
  }

  void AsyncRequest(
      const std::string& protocol, const std::string& host,
      const HttpClient::Options& options, const HttpRequest& request,
      const HttpClient::RequestOptions& request_options,
      HttpClient::ResponseInfo* response_info,
      Function<void(Expected<HttpResponse, HttpClient::ErrorCode>)>&& done)
      override {
    std::string url = protocol + "://" + host + request.uri();
    MockAsyncRequest(this, url, request.method(), *request.headers(),
                     *request.body(), response_info, &done);
  }

  using GMockActionArguments = std::tuple<
      const std::string&,  // url
      const HttpMethod&,   // method
      const HttpHeaders&,  // headers
      const std::string&,  // body
      HttpClient::ResponseInfo*,
      Function<void(Expected<HttpResponse, HttpClient::ErrorCode>)>*>;

  static void GMockActionReturn(const GMockActionArguments& arguments,
                                HttpResponse resp);

  static void GMockActionReturn(const GMockActionArguments& arguments,
                                HttpClient::ErrorCode err);

  static void GMockActionReturn(const GMockActionArguments& arguments,
                                HttpResponse resp,
                                HttpClient::ResponseInfo info);
};

}  // namespace detail

template <class T>
struct MockImplementationTraits;

template <>
struct MockImplementationTraits<flare::detail::HttpChannel> {
  using type = detail::HttpMockChannel;
};

}  // namespace flare::testing

#endif  // FLARE_TESTING_HTTP_MOCK_H_
