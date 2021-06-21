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

#include "flare/fiber/detail/fiber_entity.h"

#include "gtest/gtest.h"

namespace flare::fiber::detail {

namespace {

FiberEntity* CreateFiberEntity(SchedulingGroup* sg, bool system_fiber,
                               Function<void()>&& start_proc) noexcept {
  auto desc = NewFiberDesc();
  desc->scheduling_group_local = false;
  desc->system_fiber = system_fiber;
  desc->start_proc = std::move(start_proc);
  return InstantiateFiberEntity(sg, desc);
}

}  // namespace

class SystemFiberOrNot : public ::testing::TestWithParam<bool> {};

TEST_P(SystemFiberOrNot, GetMaster) {
  SetUpMasterFiberEntity();
  auto* fiber = GetMasterFiberEntity();
  ASSERT_TRUE(!!fiber);
  // What can we test here?
}

TEST_P(SystemFiberOrNot, CreateDestroy) {
  auto fiber = CreateFiberEntity(nullptr, GetParam(), [] {});
  ASSERT_TRUE(!!fiber);
  FreeFiberEntity(fiber);
}

TEST_P(SystemFiberOrNot, GetStackTop) {
  auto fiber = CreateFiberEntity(nullptr, GetParam(), [] {});
  ASSERT_TRUE(fiber->GetStackTop());
  FreeFiberEntity(fiber);
}

FiberEntity* master;

TEST_P(SystemFiberOrNot, Switch) {
  master = GetMasterFiberEntity();
  int x = 0;
  auto fiber = CreateFiberEntity(nullptr, GetParam(), [&] {
    x = 10;
    // Jump back to the master fiber.
    master->Resume();
  });
  fiber->Resume();

  // Back from `cb`.
  ASSERT_EQ(10, x);
  FreeFiberEntity(fiber);
}

TEST_P(SystemFiberOrNot, GetCurrent) {
  master = GetMasterFiberEntity();
  ASSERT_EQ(master, GetCurrentFiberEntity());

  FiberEntity* ptr;
  auto fiber = CreateFiberEntity(nullptr, GetParam(), [&] {
    ASSERT_EQ(GetCurrentFiberEntity(), ptr);
    GetMasterFiberEntity()->Resume();  // Equivalent to `master->Resume().`
  });
  ptr = fiber;
  fiber->Resume();

  ASSERT_EQ(master, GetCurrentFiberEntity());  // We're back.
  FreeFiberEntity(fiber);
}

TEST_P(SystemFiberOrNot, ResumeOn) {
  struct Context {
    FiberEntity* expected;
    bool tested = false;
  };
  bool fiber_run = false;
  auto fiber = CreateFiberEntity(nullptr, GetParam(), [&] {
    GetMasterFiberEntity()->Resume();
    fiber_run = true;
    GetMasterFiberEntity()->Resume();
  });

  Context ctx;
  ctx.expected = fiber;
  fiber->Resume();  // We (master fiber) will be immediately resumed by
                    // `fiber_cb`.
  fiber->ResumeOn([&] {
    ASSERT_EQ(GetCurrentFiberEntity(), ctx.expected);
    ctx.tested = true;
  });

  ASSERT_TRUE(ctx.tested);
  ASSERT_TRUE(fiber_run);
  ASSERT_EQ(master, GetCurrentFiberEntity());
  FreeFiberEntity(fiber);
}

TEST_P(SystemFiberOrNot, Fls) {
  static const std::size_t kSlots[] = {
      0,
      1,
      FiberEntity::kInlineLocalStorageSlots + 5,
      FiberEntity::kInlineLocalStorageSlots + 9999,
  };

  for (auto slot_index : kSlots) {
    auto self = GetCurrentFiberEntity();
    *self->GetFls(slot_index) = MakeErased<int>(5);

    bool fiber_run = false;
    auto fiber = CreateFiberEntity(nullptr, GetParam(), [&] {
      auto self = GetCurrentFiberEntity();
      auto fls = self->GetFls(slot_index);
      ASSERT_FALSE(*fls);

      GetMasterFiberEntity()->Resume();

      ASSERT_EQ(fls, self->GetFls(slot_index));
      *fls = MakeErased<int>(10);

      GetMasterFiberEntity()->Resume();

      ASSERT_EQ(fls, self->GetFls(slot_index));
      ASSERT_EQ(10, *reinterpret_cast<int*>(fls->Get()));
      fiber_run = true;

      GetMasterFiberEntity()->Resume();
    });

    ASSERT_EQ(self, GetMasterFiberEntity());
    auto fls = self->GetFls(slot_index);
    ASSERT_EQ(5, *reinterpret_cast<int*>(fls->Get()));

    fiber->Resume();

    ASSERT_EQ(fls, self->GetFls(slot_index));

    fiber->Resume();

    ASSERT_EQ(5, *reinterpret_cast<int*>(fls->Get()));
    ASSERT_EQ(fls, self->GetFls(slot_index));
    *reinterpret_cast<int*>(fls->Get()) = 7;

    fiber->Resume();

    ASSERT_EQ(7, *reinterpret_cast<int*>(fls->Get()));
    ASSERT_EQ(fls, self->GetFls(slot_index));

    ASSERT_TRUE(fiber_run);
    ASSERT_EQ(master, GetCurrentFiberEntity());
    FreeFiberEntity(fiber);
  }
}

TEST_P(SystemFiberOrNot, ResumeOnMaster) {
  struct Context {
    FiberEntity* expected;
    bool tested = false;
  };
  Context ctx;
  auto fiber = CreateFiberEntity(nullptr, GetParam(), [&] {
    master->ResumeOn([&] {
      ASSERT_EQ(GetCurrentFiberEntity(), ctx.expected);
      ctx.tested = true;
      // Continue running master fiber on return.
    });
  });

  ctx.expected = GetMasterFiberEntity();
  fiber->Resume();

  ASSERT_TRUE(ctx.tested);
  ASSERT_EQ(master, GetCurrentFiberEntity());
  FreeFiberEntity(fiber);
}

INSTANTIATE_TEST_SUITE_P(FiberEntity, SystemFiberOrNot,
                         ::testing::Values(true, false));

}  // namespace flare::fiber::detail
