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

#include "flare/base/id_alloc.h"

#include <algorithm>
#include <bitset>
#include <limits>
#include <set>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

#include "flare/base/chrono.h"
#include "flare/base/thread/latch.h"

using namespace std::literals;

namespace flare::id_alloc {

struct OverflowTraits {
  using Type = std::uint32_t;
  static constexpr auto kMin = 1, kMax = 1000;
  static constexpr auto kBatchSize = 10;
};

TEST(IdAlloc, Overflow) {
  std::vector<int> v(100000);
  for (auto&& e : v) {
    e = Next<OverflowTraits>();
  }
  ASSERT_TRUE(std::all_of(v.begin(), v.end(),
                          [](auto x) { return x >= 1 && x <= 1000; }));
}

struct OverflowTraits2 {
  using Type = std::int32_t;
  static constexpr auto kMin = 0x7fff'efff, kMax = 0x7fff'ffff;
  static constexpr auto kBatchSize = 10;
};

TEST(IdAlloc, Overflow2) {
  std::vector<int> v(100000);
  for (auto&& e : v) {
    e = Next<OverflowTraits2>();
  }
  ASSERT_TRUE(std::all_of(v.begin(), v.end(), [](auto x) {
    EXPECT_GE(x, 0x7fff'efff);
    EXPECT_LE(x, 0x7fff'ffff);
    return x >= 0x7fff'efff && x <= 0x7fff'ffff;
  }));
}

TEST(IdAlloc, NoDuplicateUntilOverflow) {
  constexpr auto kDistinct = 0x7fff'ffff - 0x7fff'efff;  // `kMax` is wasted`
  std::vector<int> v(100000);
  for (auto&& e : v) {
    e = Next<OverflowTraits2>();
  }
  ASSERT_EQ(kDistinct, std::set(v.begin(), v.begin() + kDistinct).size());
}

struct U32Traits {
  using Type = std::uint32_t;
  static constexpr auto kMin = 1;
  static constexpr auto kMax = std::numeric_limits<std::uint32_t>::max();
  static constexpr auto kBatchSize = 10000;
};

TEST(IdAlloc, Multithreaded) {
  constexpr auto N = 40;
  // If our optimization does work, several seconds should be enough.
  constexpr auto L = 25'000'000;
  std::vector<int> vs[N];
  std::thread ts[N];
  Latch latch(1);

  for (int i = 0; i != N; ++i) {
    vs[i].reserve(L);
  }

  auto start = ReadSteadyClock();
  for (int i = 0; i != N; ++i) {
    ts[i] = std::thread([i, &vs, &latch] {
      latch.wait();
      for (int j = 0; j != L; ++j) {
        vs[i].push_back(Next<U32Traits>());
      }
    });
  }
  latch.count_down();
  for (auto&& t : ts) {
    t.join();
  }
  std::cout << N * L << " allocs costs "
            << (ReadSteadyClock() - start) / 1ms / 1000. << " second(s)."
            << std::endl;

  // Check that no duplicate was allocated.
  static std::bitset<std::numeric_limits<std::uint32_t>::max()> seen;
  for (auto&& v : vs) {
    for (auto&& e : v) {
      ASSERT_FALSE(seen[e]);
      seen[e] = true;
    }
  }
}

}  // namespace flare::id_alloc
