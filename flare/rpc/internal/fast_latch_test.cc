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

#include "flare/rpc/internal/fast_latch.h"

#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/base/random.h"
#include "flare/fiber/async.h"
#include "flare/testing/main.h"

using namespace std::literals;

namespace flare::rpc::detail {

TEST(FastLatch, All) {
  for (int i = 0; i != 10000; ++i) {
    int x = 0;
    FastLatch fast_latch;

    auto runner = [&x, ptr = &fast_latch] {
      x = 1;
      ptr->count_down();
    };
    if (Random() % 2) {
      runner();
    } else {
      fiber::Async(runner);
    }
    fast_latch.wait();
    ASSERT_EQ(1, x);
  }
}

}  // namespace flare::rpc::detail

FLARE_TEST_MAIN
