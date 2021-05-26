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

#include "flare/io/event_loop.h"

#include <chrono>

#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/testing/main.h"

using namespace std::literals;

namespace flare {

TEST(EventLoop, Task) {
  std::atomic<int> x = 0;
  GetGlobalEventLoop(0)->AddTask([&] { x = 1; });
  while (x == 0) {
  }
  ASSERT_EQ(1, x);
}

}  // namespace flare

FLARE_TEST_MAIN
