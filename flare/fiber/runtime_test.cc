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

#include "flare/fiber/runtime.h"

#include <set>

#include "gflags/gflags.h"
#include "gtest/gtest.h"

#include "flare/base/internal/cpu.h"
#include "flare/base/thread/attribute.h"
#include "flare/base/thread/latch.h"
#include "flare/fiber/fiber.h"

DECLARE_string(flare_fiber_worker_inaccessible_cpus);

namespace flare::fiber {

TEST(Runtime, All) {
  google::FlagSaver _;
  FLAGS_flare_fiber_worker_inaccessible_cpus = "-1";

  StartRuntime();
  Latch latch(1);

  StartFiberFromPthread([&] {
    auto affinity = GetCurrentThreadAffinity();
    std::set<int> using_cpus{affinity.begin(), affinity.end()};

    // The last CPU shouldn't be usable to us.
    EXPECT_EQ(0, using_cpus.count(
                     flare::internal::GetNumberOfProcessorsConfigured() - 1));
    latch.count_down();
  });
  latch.wait();
  TerminateRuntime();
}

}  // namespace flare::fiber
