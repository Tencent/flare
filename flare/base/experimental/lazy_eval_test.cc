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

#include "flare/base/experimental/lazy_eval.h"

#include <string>

#include "gtest/gtest.h"

using namespace std::literals;

namespace flare::experimental {

TEST(LazyEval, All) {
  LazyEval<std::string> val;
  ASSERT_FALSE(val);

  val = [] { return "asdf"s; };
  ASSERT_TRUE(val);
  EXPECT_FALSE(val.IsEvaluated());
  EXPECT_EQ("asdf", val.Evaluate());
  EXPECT_TRUE(val.IsEvaluated());
  EXPECT_EQ("asdf", val.Evaluate());
  EXPECT_EQ("asdf", val.Evaluate());
  EXPECT_EQ("asdf", val.Get());

  val = "asdfg";
  ASSERT_TRUE(val);
  EXPECT_EQ("asdfg", val.Evaluate());
  EXPECT_EQ("asdfg", val.Evaluate());
}

}  // namespace flare::experimental
