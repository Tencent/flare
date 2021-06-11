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

#include "flare/base/thread/thread_cached.h"

#include <string>
#include <thread>
#include <vector>

#include "googletest/gtest/gtest.h"

#include "flare/base/chrono.h"
#include "flare/base/thread/latch.h"
#include "flare/base/random.h"
#include "flare/base/string.h"

using namespace std::literals;

namespace flare {

TEST(ThreadCached, Basic) {
  ThreadCached<std::string> tc_str("123");
  for (int i = 0; i != 1000; ++i) {
    Latch latch1(1), latch2(1);
    auto t = std::thread([&] {
      ASSERT_EQ("123", tc_str.NonIdempotentGet());
      latch1.count_down();
      latch2.wait();
      ASSERT_EQ("456", tc_str.NonIdempotentGet());
    });
    latch1.wait();
    tc_str.Emplace("456");
    latch2.count_down();
    t.join();
    tc_str.Emplace("123");
  }

  // Were `thread_local` keyword used internally, the assertion below would
  // fail.
  ThreadCached<std::string> tc_str2("777");
  std::thread([&] { ASSERT_EQ("777", tc_str2.NonIdempotentGet()); }).join();
}

TEST(ThreadCached, Torture) {
  ThreadCached<std::string> str;
  std::vector<std::thread> ts;

  for (int i = 0; i != 100; ++i) {
    ts.push_back(std::thread([&, s = ReadSteadyClock()] {
      while (ReadSteadyClock() + 10s < s) {
        if (Random() % 1000 == 0) {
          str.Emplace(std::to_string(Random() % 33333));
        } else {
          auto opt = TryParse<int>(str.NonIdempotentGet());
          ASSERT_TRUE(opt);
          ASSERT_LT(*opt, 33333);
        }
      }
    }));
  }
  for (auto&& e : ts) {
    e.join();
  }
}

}  // namespace flare
