// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "flare/base/buffer/view.h"

#include <string>
#include <string_view>

#include "googletest/gtest/gtest.h"

#include "flare/base/random.h"
#include "flare/base/string.h"

using namespace std::literals;

namespace flare {

NoncontiguousBuffer MakeAToZBuffer() {
  NoncontiguousBufferBuilder nbb;

  nbb.Append(MakeForeignBuffer("abc"));
  nbb.Append(MakeForeignBuffer("d"));
  nbb.Append("efgh");
  nbb.Append("ijk");
  nbb.Append('l');
  nbb.Append('m');
  nbb.Append(CreateBufferSlow("nopqrstuvwxyz"));
  return nbb.DestructiveGet();
}

std::string RandomString() {
  std::string str;

  for (int i = 0; i != 100; ++i) {
    str += std::to_string(i);
  }
  return str;
}

TEST(ForwardView, Basic) {
  auto buffer = MakeAToZBuffer();
  NoncontiguousBufferForwardView view(buffer);
  EXPECT_EQ(view.size(), buffer.ByteSize());
  EXPECT_FALSE(view.empty());

  char expected = 'a';
  auto iter = view.begin();
  while (iter != view.end()) {
    EXPECT_EQ(expected, *iter);
    ++expected;
    ++iter;
  }
  EXPECT_EQ('z' + 1, expected);
}

TEST(ForwardView, Search) {
  auto buffer = CreateBufferSlow(std::string(10485760, 'a'));
  NoncontiguousBufferForwardView view(buffer);
  constexpr auto kFound = "aaaaaaaaaaaaaaaaaaaaaaaaaaa"sv,
                 kNotFound = "aaaaaaaaaaaaaaaaaaaaab"sv;
  EXPECT_EQ(view.begin(), std::search(view.begin(), view.end(), kFound.begin(),
                                      kFound.end()));
  EXPECT_EQ(view.end(), std::search(view.begin(), view.end(), kNotFound.begin(),
                                    kNotFound.end()));
}

TEST(RandomView, Basic) {
  auto buffer = MakeAToZBuffer();
  NoncontiguousBufferRandomView view(buffer);
  EXPECT_EQ(view.size(), buffer.ByteSize());
  EXPECT_FALSE(view.empty());

  char expected = 'a';
  auto iter = view.begin();
  while (iter != view.end()) {
    EXPECT_EQ(expected, *iter);
    ++expected;
    ++iter;
  }
  EXPECT_EQ('z' + 1, expected);
  for (int i = 'a'; i <= 'z'; ++i) {
    auto iter = view.begin();
    iter = iter + (i - 'a');
    EXPECT_EQ(i, *iter);
    EXPECT_EQ(i - 'a', iter - view.begin());
  }
  iter = view.begin();
  iter += 'z' - 'a' + 1;  // to `end()`.
  EXPECT_EQ(iter, view.end());
}

TEST(RandomView, Search0) {
  auto buffer = CreateBufferSlow("");
  NoncontiguousBufferRandomView view(buffer);
  constexpr auto kKey = "aaaaaaaaaaaaaaaaaaaaaaaaaaa"sv;
  auto result = std::search(view.begin(), view.end(), kKey.begin(), kKey.end());
  EXPECT_EQ(view.begin(), result);
}

TEST(RandomView, Search1) {
  auto buffer = CreateBufferSlow(std::string(10485760, 'a'));
  NoncontiguousBufferRandomView view(buffer);
  constexpr auto kFound = "aaaaaaaaaaaaaaaaaaaaaaaaaaa"sv,
                 kNotFound = "aaaaaaaaaaaaaaaaaaaaab"sv;
  auto result1 =
      std::search(view.begin(), view.end(), kFound.begin(), kFound.end());
  EXPECT_EQ(view.begin(), result1);
  EXPECT_EQ(0, result1 - view.begin());
  auto result2 =
      std::search(view.begin(), view.end(), kNotFound.begin(), kNotFound.end());
  EXPECT_EQ(view.end(), result2);
  EXPECT_EQ(view.size(), result2 - view.begin());
}

TEST(RandomView, Search2) {
  auto buffer = MakeAToZBuffer();
  NoncontiguousBufferRandomView view(buffer);
  constexpr auto kFound = "hijklmn"sv;
  auto result =
      std::search(view.begin(), view.end(), kFound.begin(), kFound.end());
  EXPECT_EQ(7, result - view.begin());
}

TEST(RandomView, RandomSearch) {
  for (int i = 0; i != 100000; ++i) {
    auto value = RandomString();
    auto temp = Format("asdfdsf{}XXXADFFDAF", value);
    auto start = 0;

    NoncontiguousBufferBuilder builder;
    while (start != temp.size()) {
      auto size = Random<int>(1, temp.size() - start);
      builder.Append(temp.substr(start, size));
      start += size;
    }
    auto buffer = builder.DestructiveGet();

    NoncontiguousBufferRandomView view(buffer);
    auto result =
        std::search(view.begin(), view.end(), value.begin(), value.end());
    if (result - view.begin() != 7) {
      abort();
    }
    ASSERT_EQ(7, result - view.begin());
  }
}

}  // namespace flare
