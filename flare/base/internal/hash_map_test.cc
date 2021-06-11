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

#include "flare/base/internal/hash_map.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "googletest/gtest/gtest.h"

#include "flare/base/random.h"

namespace flare::internal {

bool HashMapEqual(const HashMap<int, std::string>& x,
                  const HashMap<int, std::string>& y) {
  std::vector<std::pair<int, std::string>> v1(x.begin(), x.end()),
      v2(y.begin(), y.end());
  std::sort(v1.begin(), v1.end());
  std::sort(v2.begin(), v2.end());
  return v1 == v2;
}

TEST(HashMap, Easy) {
  HashMap<int, std::string> m;

  m[1] = "1";
  m[10] = "10";
  m[-5] = "-5";
  ASSERT_FALSE(m.TryGet(3));
  m[3] = "3";

  ASSERT_TRUE(m.TryGet(1));
  ASSERT_TRUE(m.TryGet(10));
  ASSERT_TRUE(m.TryGet(-5));
  ASSERT_TRUE(m.TryGet(3));

  ASSERT_EQ("3", *m.TryGet(3));
  ASSERT_EQ("3", m[3]);
}

TEST(HashMap, Random) {
  constexpr auto kIterations = 5000000;
  constexpr auto kMaxKey = kIterations / 10;
  HashMap<int, std::string> m1;
  std::map<int, std::string> m2;
  HashMap<int, std::string> map_copy;
  HashMap<int, std::string> map_move;

  for (int i = 0; i != kIterations; ++i) {
    auto op = Random() % 4;
    auto k = Random(kMaxKey);
    if (op == 1) {  // Insertion.
      auto v = std::to_string(Random());
      m1[k] = v;
      m2[k] = v;
    } else if (op == 2) {  // Erasure.
      auto c1 = m1.erase(k);
      auto c2 = m2.erase(k);
      ASSERT_EQ(c1, c2);
    } else {  // Find.
      std::string *p1, *p2 = p1 = nullptr;
      if (Random() & 1) {  // TryGet.
        p1 = m1.TryGet(k);
      } else {
        if (auto iter = m1.find(k); iter != m1.end()) {
          p1 = &iter->second;
        }
      }
      if (auto iter = m2.find(k); iter != m2.end()) {
        p2 = &iter->second;
      }
      if (p1) {
        ASSERT_EQ(*p1, *p2);
      }
    }
    ASSERT_EQ(m1.size(), m2.size());
    if (Random() % 100000 == 0) {
      map_copy = m1;
      ASSERT_TRUE(HashMapEqual(map_copy, m1));
      map_move = std::move(map_copy);
      ASSERT_TRUE(HashMapEqual(map_move, m1));
      ASSERT_TRUE(map_copy.empty());
    }
  }
}

TEST(HashMap, DeletionAfterInsertion) {
  for (int j = 0; j != 100; ++j) {
    constexpr auto kIterations = 100000;
    constexpr auto kMaxKey = kIterations / 10;
    HashMap<int, std::string> m1;
    std::map<int, std::string> m2;

    for (int i = 0; i != kIterations; ++i) {
      auto k = Random(kMaxKey);
      auto v = std::to_string(Random());
      m1[k] = v;
      m2[k] = v;
      ASSERT_EQ(m1.size(), m2.size());
    }

    for (int i = 0; i != kIterations; ++i) {
      auto k = Random(kMaxKey);
      auto c1 = m1.erase(k);
      auto c2 = m2.erase(k);
      ASSERT_EQ(c1, c2);
      ASSERT_EQ(m1.size(), m2.size());
    }
  }
}

struct NotEquallyComparable {
  std::string s;
};

struct NotEquallyComparableHash {
  std::size_t operator()(const NotEquallyComparable& x) const {
    return Hash<std::string>()(x.s);
  }
};

struct NotEquallyComparableEqualTo {
  bool operator()(const NotEquallyComparable& x,
                  const NotEquallyComparable& y) const {
    return x.s == y.s;
  }
};

TEST(HashMap, UserDefinedEqualTo) {
  HashMap<NotEquallyComparable, int, NotEquallyComparableHash,
          NotEquallyComparableEqualTo>
      m;
  m[NotEquallyComparable{"a"}] = 1;
  m[NotEquallyComparable{"b"}] = 2;
  ASSERT_EQ(2, m.size());
  ASSERT_EQ(1, m[NotEquallyComparable{"a"}]);
}

}  // namespace flare::internal
