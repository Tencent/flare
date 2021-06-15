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

#include "flare/base/function_view.h"

#include "gtest/gtest.h"

namespace flare {

int PlainOldFunction(int, double, char) { return 1; }

int CallThroughFunctionView(FunctionView<int(int, double, char)> fv, int x,
                            double y, char z) {
  return fv(x, y, z);
}

int CallThroughFunctionView2(FunctionView<int()> fv) { return fv(); }

TEST(FunctionViewTest, POF) {
  ASSERT_EQ(1, (CallThroughFunctionView(PlainOldFunction, 0, 0, 0)));
}

TEST(FunctionViewTest, POFPtr) {
  ASSERT_EQ(1, (CallThroughFunctionView(&PlainOldFunction, 0, 0, 0)));
}

TEST(FunctionViewTest, Lambda) {
  ASSERT_EQ(1, CallThroughFunctionView2([] { return 1; }));
}

struct ConstOperatorCall {
  int operator()() const { return 1; }
};

struct NonconstOperatorCall {
  int operator()() { return 1; }
};

struct NonconstNoexceptOperatorCall {
  int operator()() noexcept { return 1; }
};

TEST(FunctionViewTest, ConstnessCorrect) {
  ASSERT_EQ(1, CallThroughFunctionView2(ConstOperatorCall()));
  ASSERT_EQ(1, CallThroughFunctionView2(NonconstOperatorCall()));
  ASSERT_EQ(1, CallThroughFunctionView2(NonconstNoexceptOperatorCall()));
}

class FancyClass {
 public:
  int f(int x) { return x; }
};

int CallThroughFunctionView3(FunctionView<int(FancyClass*, int)> fv,
                             FancyClass* fc, int x) {
  return fv(fc, x);
}

TEST(FunctionViewTest, MemberMethod) {
  FancyClass fc;
  ASSERT_EQ(10, CallThroughFunctionView3(&FancyClass::f, &fc, 10));
}

void CallThroughFunctionView4(FunctionView<void()> f) { f(); }

TEST(FunctionViewTest, CastAnyTypeToVoid) {
  int x = 0;
  CallThroughFunctionView4([&x]() -> int {
    x = 1;
    return x;
  });

  ASSERT_EQ(1, x);
}

}  // namespace flare
