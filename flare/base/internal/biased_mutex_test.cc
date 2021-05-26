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

#include "flare/base/internal/biased_mutex.h"

#include <chrono>
#include <thread>

#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/base/chrono.h"

using namespace std::literals;

namespace flare::internal {

TEST(BiasedMutex, All) {
  struct alignas(128) AlignedInt {
    std::int64_t v;
  };
  AlignedInt separated_counters[3] = {};
  std::atomic<bool> leave{false};

  BiasedMutex biased_mutex;
  // Both protected by `biased_mutex`.
  std::int64_t v = 0;
  std::int64_t v_copy;

  std::thread blessed([&] {
    while (!leave.load(std::memory_order_relaxed)) {
      ++separated_counters[0].v;
      std::scoped_lock _(*biased_mutex.blessed_side());
      ++v;
      v_copy = v;
    }
  });
  std::thread really_slow([&] {
    auto start = ReadSteadyClock();
    while (ReadSteadyClock() - start < 10s) {
      ++separated_counters[1].v;
      std::scoped_lock _(*biased_mutex.really_slow_side());
      ++v;
      v_copy = v;
    }
  });
  std::thread really_slow2([&] {
    auto start = ReadSteadyClock();
    while (ReadSteadyClock() - start < 10s) {
      ++separated_counters[2].v;
      std::scoped_lock _(*biased_mutex.really_slow_side());
      ++v;
      v_copy = v;
    }
  });

  really_slow.join();
  really_slow2.join();
  leave = true;
  blessed.join();

  std::scoped_lock _(*biased_mutex.really_slow_side());
  ASSERT_EQ(v, separated_counters[0].v + separated_counters[1].v +
                   separated_counters[2].v);
  ASSERT_EQ(v, v_copy);
}

}  // namespace flare::internal
