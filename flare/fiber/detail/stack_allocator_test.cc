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

#include "flare/fiber/detail/stack_allocator.h"

#include <unistd.h>

#include "thirdparty/gflags/gflags.h"
#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/base/internal/annotation.h"
#include "flare/base/logging.h"

DECLARE_int32(flare_fiber_stack_size);

namespace flare::fiber::detail {

// If ASan is present, the death test below triggers ASan and aborts.
#ifndef FLARE_INTERNAL_USE_ASAN

TEST(StackAllocatorDeathTest, SystemStackCanaryValue) {
  auto stack = CreateSystemStack();
  ASSERT_TRUE(stack);
  memset(stack, 0, 8192);  // Canary value is overwritten.
  ASSERT_DEATH(FreeSystemStack(stack), "stack is corrupted");
}

#endif

TEST(StackAllocator, UserStack) {
  auto stack = CreateUserStack();
  ASSERT_TRUE(stack);
  memset(stack, 0, FLAGS_flare_fiber_stack_size);
  FreeUserStack(stack);
}

#ifndef FLARE_INTERNAL_USE_ASAN

TEST(StackAllocator, SystemStack) {
  auto stack = CreateSystemStack();
  ASSERT_TRUE(stack);
  memset(reinterpret_cast<char*>(stack) + 16 /* Canary value */, 0,
         kSystemStackSize - 16);
  FreeSystemStack(stack);
}

#else

TEST(StackAllocator, SystemStack) {
  auto stack = CreateSystemStack();
  ASSERT_TRUE(stack);
  memset(reinterpret_cast<char*>(stack) +
             kSystemStackPoisonedSize /* Poisoned page */,
         0, kSystemStackSize - kSystemStackPoisonedSize);
  FreeSystemStack(stack);
}

#endif

}  // namespace flare::fiber::detail
