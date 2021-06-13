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

#include "flare/fiber/execution_context.h"

#include "gtest/gtest.h"

#include "flare/fiber/async.h"
#include "flare/fiber/future.h"
#include "flare/fiber/latch.h"
#include "flare/fiber/timer.h"
#include "flare/testing/main.h"

using namespace std::literals;

namespace flare::fiber {

TEST(ExecutionContext, NullContext) {
  ASSERT_EQ(nullptr, ExecutionContext::Current());
}

TEST(ExecutionContext, RunInContext) {
  ASSERT_EQ(nullptr, ExecutionContext::Current());
  auto ctx = ExecutionContext::Create();
  static ExecutionLocal<int> els_int;

  ctx->Execute([&] {
    ASSERT_EQ(ExecutionContext::Current(), ctx.Get());
    *els_int = 10;
    ASSERT_EQ(10, *els_int);
  });
  ctx->Execute([&] {
    ASSERT_EQ(ExecutionContext::Current(), ctx.Get());
    ASSERT_EQ(10, *els_int);
  });

  auto ctx2 = ExecutionContext::Create();
  ctx2->Execute([&] {
    ASSERT_EQ(ExecutionContext::Current(), ctx2.Get());
    *els_int = 5;
    ASSERT_EQ(5, *els_int);
  });
  ctx->Execute([&] {
    ASSERT_EQ(ExecutionContext::Current(), ctx.Get());
    ASSERT_EQ(10, *els_int);
  });
  ctx2->Execute([&] {
    ASSERT_EQ(ExecutionContext::Current(), ctx2.Get());
    ASSERT_EQ(5, *els_int);
  });
}

TEST(ExecutionContext, AsyncPropagation) {
  ASSERT_EQ(nullptr, ExecutionContext::Current());
  auto ctx = ExecutionContext::Create();
  static ExecutionLocal<int> els_int;

  ctx->Execute([&] {
    *els_int = 10;
    auto f1 = fiber::Async([&] {
      ASSERT_EQ(10, *els_int);
      fiber::BlockingGet(Async([&] { ASSERT_EQ(10, *els_int); }));
    });
    auto f2 = fiber::Async([&] { ASSERT_EQ(10, *els_int); });
    fiber::BlockingGet(WhenAll(&f1, &f2));
  });
}

TEST(ExecutionContext, TimerPropagation) {
  ASSERT_EQ(nullptr, ExecutionContext::Current());
  auto ctx = ExecutionContext::Create();
  static ExecutionLocal<int> els_int;

  ctx->Execute([&] {
    *els_int = 10;
    fiber::Latch latch(2);
    SetDetachedTimer(ReadCoarseSteadyClock() + 100ms, [&] {
      ASSERT_EQ(10, *els_int);
      latch.count_down();
    });
    SetDetachedTimer(ReadCoarseSteadyClock() + 50ms, [&] {
      ASSERT_EQ(10, *els_int);
      latch.count_down();
    });
    latch.wait();
  });
}

TEST(ExecutionLocal, All) {
  ASSERT_EQ(nullptr, ExecutionContext::Current());
  auto ctx = ExecutionContext::Create();
  static ExecutionLocal<int> els_int;
  static ExecutionLocal<int> els_int2;
  static ExecutionLocal<double> els_dbl;

  ctx->Execute([&] {
    ASSERT_EQ(ExecutionContext::Current(), ctx.Get());
    *els_int = 10;
    *els_int2 = 11;
    *els_dbl = 12;
  });
  ctx->Execute([&] {
    ASSERT_EQ(ExecutionContext::Current(), ctx.Get());
    ASSERT_EQ(10, *els_int);
    ASSERT_EQ(11, *els_int2);
    ASSERT_EQ(12, *els_dbl);
  });

  auto ctx2 = ExecutionContext::Create();
  ctx2->Execute([&] {
    ASSERT_EQ(ExecutionContext::Current(), ctx2.Get());
    *els_int = 0;
    *els_int2 = 1;
    *els_dbl = 2;
  });
  ctx2->Execute([&] {
    ASSERT_EQ(ExecutionContext::Current(), ctx2.Get());
    ASSERT_EQ(0, *els_int);
    ASSERT_EQ(1, *els_int2);
    ASSERT_EQ(2, *els_dbl);
  });
  ctx->Execute([&] {
    ASSERT_EQ(ExecutionContext::Current(), ctx.Get());
    ASSERT_EQ(10, *els_int);
    ASSERT_EQ(11, *els_int2);
    ASSERT_EQ(12, *els_dbl);
  });
  ctx2->Execute([&] {
    ASSERT_EQ(ExecutionContext::Current(), ctx2.Get());
    ASSERT_EQ(0, *els_int);
    ASSERT_EQ(1, *els_int2);
    ASSERT_EQ(2, *els_dbl);
  });
}

}  // namespace flare::fiber

FLARE_TEST_MAIN
