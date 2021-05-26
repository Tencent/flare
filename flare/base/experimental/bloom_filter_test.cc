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

#include "flare/base/experimental/bloom_filter.h"

#include <iostream>
#include <string>
#include <unordered_set>

#include "thirdparty/googletest/gtest/gtest.h"

namespace flare::experimental {

// 256 bytes.
constexpr std::string_view kVeryLongString =
    "1234567890123456789012345678901234567890123456789012345678901234"
    "1234567890123456789012345678901234567890123456789012345678901234"
    "1234567890123456789012345678901234567890123456789012345678901234"
    "1234567890123456789012345678901234567890123456789012345678901234";

std::string GetRandomStringOfLength(std::size_t length) {
  std::string s;
  while (s.size() < length) {
    auto random = Random<std::uint64_t>();  // 8 bytes.
    s.append(reinterpret_cast<char*>(&random), 8);
  }
  while (s.size() > length) {
    s.pop_back();
  }
  return s;
}

TEST(BloomFilter, Basic) {
  BloomFilter filter(12345, 1e-6, 4);
  constexpr std::string_view kAdded[] = {
      "These", "are", "added", "to", "the", "Bloom", "Filter", kVeryLongString};
  constexpr std::string_view kNotExisting[] = {"But", "not", "us"};

  EXPECT_EQ(4, filter.GetIterationCount());
  for (auto&& e : kAdded) {
    filter.Add(e);
  }
  for (auto&& e : kAdded) {
    EXPECT_TRUE(filter.PossiblyContains(e));
  }
  for (auto&& e : kNotExisting) {
    EXPECT_FALSE(filter.PossiblyContains(e));
  }

  // Copied.
  BloomFilter filter2(filter.GetBytes(), filter.GetIterationCount());
  for (auto&& e : kAdded) {
    filter2.Add(e);
  }
  for (auto&& e : kAdded) {
    EXPECT_TRUE(filter2.PossiblyContains(e));
  }
  for (auto&& e : kNotExisting) {
    EXPECT_FALSE(filter2.PossiblyContains(e));
  }
}

TEST(BloomFilter, Random) {
  constexpr auto kSize = 123456;
  BloomFilter filter(kSize, 1e-6, 8);
  std::unordered_set<std::string> inserted;

  FLARE_LOG_INFO("Filter size: {} bits", filter.GetBytes().size());

  for (int i = 0; i != kSize; ++i) {
    auto s = GetRandomStringOfLength(Random(1, 10));
    if (inserted.count(s)) {
      continue;
    }
    filter.Add(s);
    inserted.insert(s);
  }

  constexpr auto kTests = 1234567;
  auto false_positives = 0;
  for (int i = 0; i != kTests; ++i) {
    auto s = GetRandomStringOfLength(Random(1, 10));
    false_positives += (inserted.count(s) == 0 && filter.PossiblyContains(s));
  }

  std::cout << "False positives: " << false_positives << std::endl;
  EXPECT_LT(false_positives, kTests * 1e-6 + 10 /* Some tolerance. */);
}

TEST(BloomFilter, Merge) {
  constexpr auto kSize = 123456;
  BloomFilter filter1(kSize, 1e-6, 8);
  BloomFilter filter2(filter1.GetBytes().size() * 8,
                      filter1.GetIterationCount());
  std::unordered_set<std::string> inserted;

  for (int i = 0; i != kSize; ++i) {
    auto s = GetRandomStringOfLength(Random(1, 10));
    if (inserted.count(s)) {
      continue;
    }
    if (Random(1) == 0) {
      filter1.Add(s);
    } else {
      filter2.Add(s);
    }
    inserted.insert(s);
  }

  BloomFilter filter(filter1.GetBytes(), filter1.GetIterationCount());
  filter.MergeFrom(filter2);

  constexpr auto kTests = 1234567;
  auto false_positives = 0;
  for (int i = 0; i != kTests; ++i) {
    auto s = GetRandomStringOfLength(Random(1, 10));
    false_positives += (inserted.count(s) == 0 && filter.PossiblyContains(s));
  }

  std::cout << "False positives: " << false_positives << std::endl;
  EXPECT_LT(false_positives, kTests * 1e-6 + 10 /* Some tolerance. */);
}

}  // namespace flare::experimental
