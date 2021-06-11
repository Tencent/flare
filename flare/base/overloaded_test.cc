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

#include "flare/base/overloaded.h"

#include <string>
#include <variant>

#include "googletest/gtest/gtest.h"

using namespace std::literals;

namespace flare {

TEST(Overloaded, All) {
  std::variant<int, double, bool, std::string> v;
  int x = 0;
  double y = 0;
  bool z = false;
  std::string a;

  auto visitor =
      Overloaded{[&](int v) { x = v; }, [&](double v) { y = v; },
                 [&](bool v) { z = v; }, [&](const std::string& v) { a = v; }};

  v = "asdf"s;
  std::visit(visitor, v);
  ASSERT_EQ("asdf", a);

  v = 1;
  std::visit(visitor, v);
  ASSERT_EQ(1, x);

  v = 1.0;
  std::visit(visitor, v);
  ASSERT_EQ(1.0, y);

  v = true;
  std::visit(visitor, v);
  ASSERT_EQ(true, z);
}

}  // namespace flare
