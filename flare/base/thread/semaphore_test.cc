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

#include "flare/base/thread/semaphore.h"

#include <atomic>
#include <thread>
#include <vector>

#include "thirdparty/googletest/gtest/gtest.h"

namespace flare {

std::atomic<int> counter{};

TEST(Semaphore, All) {
  for (int j = 0; j != 100; ++j) {
    CountingSemaphore semaphore(100);
    std::vector<std::thread> ts;

    for (int i = 0; i != 10000; ++i) {
      ts.emplace_back([&] {
        semaphore.acquire();
        ++counter;
        EXPECT_LE(counter, 100);
        --counter;
        semaphore.release();
      });
    }

    for (auto&& e : ts) {
      e.join();
    }
  }
}

}  // namespace flare
