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

#include "flare/base/internal/circular_buffer.h"

#include <string>
#include <thread>

#include "gtest/gtest.h"

namespace flare::internal {

TEST(CircularBuffer, Capacity) {
  static constexpr auto kCapacity = 12345;
  CircularBuffer<std::string> buffer(kCapacity);

  for (int i = 0; i != kCapacity; ++i) {
    ASSERT_TRUE(buffer.Emplace("asdf"));
  }
  ASSERT_FALSE(buffer.Emplace("asdf"));
}

TEST(CircularBuffer, Torture) {
  CircularBuffer<std::string> buffer(10000);
  static constexpr auto kObjectsToPush = 100'000'000;

  std::thread prod([&] {
    std::size_t pushed = 0;

    while (pushed != kObjectsToPush) {
      pushed += buffer.Emplace("my fancy string") ? 1 : 0;
    }
  });

  std::thread consumer([&] {
    std::size_t consumed = 0;

    while (consumed != kObjectsToPush) {
      thread_local std::vector<std::string> objects;
      objects.clear();
      buffer.Pop(&objects);
      for (auto&& e : objects) {
        ASSERT_EQ("my fancy string", e);
      }
      consumed += objects.size();
      ASSERT_LE(consumed, kObjectsToPush);
    }
  });

  prod.join();
  consumer.join();
}

}  // namespace flare::internal
