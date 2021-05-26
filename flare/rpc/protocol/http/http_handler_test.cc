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

#include "thirdparty/googletest/gmock/gmock.h"
#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/base/down_cast.h"
#include "flare/testing/main.h"

namespace flare {

class Server;

class TestHandler : public flare::HttpHandler {
 public:
  explicit TestHandler(Server* owner = nullptr) {}
};

FLARE_RPC_SERVER_REGISTER_BUILTIN_HTTP_HANDLER(TestHandler, "/test/handler1",
                                               "/test/handler2");

FLARE_RPC_SERVER_REGISTER_BUILTIN_HTTP_PREFIX_HANDLER(
    [](auto&& owner) { return std::make_unique<TestHandler>(); }, "/prefix");

TEST(HttpHandler, MethodNotAllowed) {
  HttpRequest req;
  req.set_method(HttpMethod::Post);

  HttpResponse resp;
  auto handler = NewHttpGetHandler([](auto&&...) { CHECK(!"Can't be here."); });

  handler->HandleRequest(req, &resp, {});
  ASSERT_EQ(HttpStatus::MethodNotAllowed, resp.status());
}

TEST(BuiltinHandlerRegistry, All) {
  auto&& handlers = detail::GetBuiltinHttpHandlers();
  EXPECT_EQ(1, handlers.size());
  EXPECT_THAT(handlers[0].second,
              ::testing::ElementsAre("/test/handler1", "/test/handler2"));

  auto&& handler_registry = detail::GetBuiltinHttpPrefixHandlers();
  EXPECT_EQ(1, handler_registry.size());
  EXPECT_EQ("/prefix", handler_registry[0].second);
}

}  // namespace flare

FLARE_TEST_MAIN
