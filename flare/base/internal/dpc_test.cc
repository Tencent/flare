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

#include "flare/base/internal/dpc.h"

#include <atomic>
#include <chrono>
#include <vector>

#include "gtest/gtest.h"

#include "flare/base/write_mostly.h"
#include "flare/fiber/fiber.h"
#include "flare/fiber/runtime.h"
#include "flare/testing/main.h"

using namespace std::literals;

namespace flare::internal {

std::atomic<std::uint64_t> count{};
WriteMostlyCounter<std::uint64_t> count2{};

TEST(Dpc, All) {
  std::uint64_t last_read = 0;
  for (int i = 0; i != 3; ++i) {
    auto start = ReadSteadyClock();
    std::vector<Fiber> workers;
    for (int i = 0; i != 4; ++i) {
      Fiber::Attributes attr;
      attr.scheduling_group = i % fiber::GetSchedulingGroupCount();
      attr.scheduling_group_local = true;

      workers.push_back(Fiber(attr, [] {
        std::uint64_t add = 0;
        auto end = ReadCoarseSteadyClock() + 10s;
        while (ReadCoarseSteadyClock() < end) {
          QueueDpc([] { count2.Add(1); });
          ++add;
        }
        count += add;
      }));
    }

    for (auto&& e : workers) {
      e.join();
    }

    while (count != count2.Read()) {
      std::this_thread::sleep_for(1ms);
    }

    ASSERT_EQ(count, count2.Read());
    FLARE_LOG_INFO(
        "{} DPCs per second per thread.",
        (count - last_read) / 60 / ((ReadSteadyClock() - start) / 1s));
    last_read = count;
  }
}

}  // namespace flare::internal

FLARE_TEST_MAIN
