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

#include "flare/base/internal/thread_pool.h"

#include "thirdparty/googletest/gtest/gtest.h"

using namespace std::literals;

namespace flare::internal {

TEST(ThreadPool, All) {
  for (int nice_value : {0, 10}) {
    std::atomic<int> ct{};
    ThreadPool tp(10, std::vector<int>(), nice_value);
    for (int i = 0; i != 1000; ++i) {
      tp.QueueJob([&] {
        for (int j = 0; j != 1000; ++j) {
          tp.QueueJob([&] { ++ct; });
        }
      });
    }
    while (ct != 1000 * 1000) {
      // NOTHING.
    }
    tp.Stop();
    tp.Join();
    ASSERT_EQ(1000 * 1000, ct);  // ...
  }
}

}  // namespace flare::internal
