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

#include "flare/base/experimental/flyweight.h"

#include <string>
#include <unordered_map>

#include "thirdparty/googletest/gtest/gtest.h"

using namespace std::literals;

namespace flare::experimental {

TEST(Flyweight, All) {
  auto x = MakeFlyweight<std::string>("hello world");
  auto y = MakeFlyweight<std::string>(
      {'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd'});
  auto z = MakeFlyweight<std::string>("hi world");
  EXPECT_TRUE(x);
  EXPECT_EQ(x, y);
  EXPECT_EQ(*x, *y);
  EXPECT_NE(x, z);
  EXPECT_EQ("hello world", *x);
  EXPECT_EQ("hello world"s, x->c_str());

  std::unordered_map<Flyweight<std::string>, bool> m;
  m[x] = true;
  EXPECT_TRUE(m[x]);
  EXPECT_TRUE(m[y]);
  EXPECT_FALSE(m[z]);

  Flyweight<int> f;
  EXPECT_FALSE(f);
}

}  // namespace flare::experimental
