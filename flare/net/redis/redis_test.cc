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

#include <chrono>

#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/base/string.h"
#include "flare/fiber/fiber.h"
#include "flare/net/redis/redis_client.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"
#include "flare/testing/naked_server.h"

using namespace std::literals;

namespace flare {

std::vector<std::string> ParseCommand(NoncontiguousBuffer* buffer) {
  auto flatten = FlattenSlow(*buffer);
  if (flatten.empty()) {
    return {};
  }
  std::string_view view = flatten;
  FLARE_CHECK_EQ('*', view.front());  // Number of strings in request.
  view.remove_prefix(1);

  auto pos = view.find("\r\n");
  if (pos == std::string_view::npos) {
    return {};  // More bytes required.
  }
  auto strings = TryParse<int>(view.substr(0, pos));
  FLARE_CHECK(strings);
  view.remove_prefix(pos + 2);

  std::vector<std::string> result;
  for (int i = 0; i != *strings; ++i) {
    FLARE_CHECK_EQ('$', view.front());
    view.remove_prefix(1);
    auto pos = view.find("\r\n");
    if (pos == std::string_view::npos) {
      return {};
    }
    auto length = TryParse<int>(view.substr(0, pos));
    FLARE_CHECK(length);
    view.remove_prefix(pos + 2);
    if (view.size() < *length + 2 /* \r\n */) {
      return {};
    }
    result.push_back(std::string(view.substr(0, *length)));
    view.remove_prefix(*length + 2);
  }
  buffer->Skip(flatten.size() - view.size());
  return result;
}

std::mutex our_kv_store_lock;
std::map<std::string, std::string> our_kv_store;

bool RedisHandler(StreamConnection* conn, NoncontiguousBuffer* buffer) {
  while (true) {
    auto command = ParseCommand(buffer);
    if (command.empty()) {
      break;
    }
    if (command[0] == "AUTH") {
      FLARE_CHECK(conn->Write(CreateBufferSlow("+OK\r\n"), 0));
    } else if (command[0] == "SET") {
      FLARE_CHECK_EQ(command.size(), 3);
      {
        std::scoped_lock _(our_kv_store_lock);
        our_kv_store[command[1]] = command[2];
      }
      FLARE_CHECK(conn->Write(CreateBufferSlow("+OK\r\n"), 0));
    } else if (command[0] == "GET") {
      std::string resp;
      {
        std::scoped_lock _(our_kv_store_lock);
        if (auto iter = our_kv_store.find(command[1]);
            iter != our_kv_store.end()) {
          resp = Format("${}\r\n{}\r\n", iter->second.size(), iter->second);
        } else {
          resp = "$-1\r\n";
        }
      }
      FLARE_CHECK(conn->Write(CreateBufferSlow(resp), 0));
    } else {
      FLARE_UNEXPECTED();
    }
  }
  return true;
}

class WithPassword : public ::testing::TestWithParam<std::string> {};

TEST_P(WithPassword, All) {
  FLARE_LOG_INFO("Testing with password [{}].", GetParam());

  auto server_ep = testing::PickAvailableEndpoint();
  testing::NakedServer server;
  server.SetHandler(RedisHandler);
  server.ListenOn(server_ep);
  server.Start();

  RedisChannel channel("redis://" + server_ep.ToString(),
                       RedisChannel::Options{.password = GetParam()});
  RedisClient client(&channel);
  Fiber fibers[1000];
  for (auto&& e : fibers) {
    e = Fiber([&] {
      auto result = client.Execute(RedisCommand("SET", "mykey", "12345"), 20s);
      ASSERT_EQ("OK", *result.as<RedisString>());
      result = client.Execute(RedisCommand("GET", "mykey"), 20s);
      ASSERT_TRUE(result.is<RedisBytes>());
      EXPECT_EQ("12345", FlattenSlow(*result.as<RedisBytes>()));
      result = client.Execute(RedisCommand("GET", "404"), 20s);
      ASSERT_TRUE(result.is<RedisNull>());
    });
  }
  for (auto&& e : fibers) {
    e.join();
  }
}

INSTANTIATE_TEST_SUITE_P(Redis, WithPassword,
                         ::testing::Values(""s, "some pass"s));

}  // namespace flare

FLARE_TEST_MAIN
