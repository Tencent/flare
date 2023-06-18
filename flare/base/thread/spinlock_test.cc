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

#include "flare/base/thread/spinlock.h"

#include <thread>

#include "gtest/gtest.h"

#include "flare/base/thread/latch.h"

namespace flare {

std::uint64_t counter{};

TEST(Spinlock, All) {
  constexpr auto T = 100;
  constexpr auto N = 100000;
  std::thread ts[100];
  Latch latch(1);
  Spinlock splk;

  for (auto&& t : ts) {
    t = std::thread([&] {
      latch.wait();
      for (int i = 0; i != N; ++i) {
        std::scoped_lock lk(splk);
        ++counter;
      }
    });
  }
  latch.count_down();
  for (auto&& t : ts) {
    t.join();
  }
  ASSERT_EQ(T * N, counter);
}

}  // namespace flare
