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

#include "flare/fiber/detail/run_queue.h"

#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>

#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/base/thread/latch.h"
#include "flare/base/random.h"

using namespace std::literals;

namespace flare::fiber::detail {

FiberEntity* CreateEntity(int x) { return reinterpret_cast<FiberEntity*>(x); }

TEST(RunQueue, Basics) {
  RunQueue queue(32);
  ASSERT_TRUE(queue.Push(CreateEntity(3), false));
  ASSERT_FALSE(queue.UnsafeEmpty());
  ASSERT_EQ(CreateEntity(3), queue.Pop());
}

TEST(RunQueue, Steal) {
  RunQueue queue(32);
  ASSERT_TRUE(queue.Push(CreateEntity(3), false));
  ASSERT_FALSE(queue.UnsafeEmpty());
  ASSERT_EQ(CreateEntity(3), queue.Steal());
}

TEST(RunQueue, Nonstealable) {
  RunQueue queue(32);
  ASSERT_TRUE(queue.Push(CreateEntity(3), true));
  ASSERT_FALSE(queue.UnsafeEmpty());
  ASSERT_FALSE(queue.Steal());
  ASSERT_EQ(CreateEntity(3), queue.Pop());
}

TEST(RunQueue, Torture) {
  constexpr auto N = 1'000'000;

  RunQueue queue(1048576);

  // Loop for several rounds so that we can test the case when queue's internal
  // ring buffer wraps around.
  for (int k = 0; k != 10; ++k) {
    constexpr auto T = 200;
    std::thread ts[T];
    Latch latch(T);
    std::mutex lock;
    std::vector<FiberEntity*> rcs;
    std::atomic<std::size_t> read = 0;
    static_assert(N % T == 0 && T % 2 == 0);
    for (int i = 0; i != T / 2; ++i) {
      ts[i] = std::thread([s = N / (T / 2) * i, &queue, &latch] {
        auto as_batch = Random() % 2 == 0;
        if (as_batch) {
          std::vector<FiberEntity*> fbs;
          for (int i = 0; i != N / (T / 2); ++i) {
            fbs.push_back(CreateEntity(s + i + 1));
          }
          latch.count_down();
          latch.wait();
          for (auto iter = fbs.begin(); iter < fbs.end();) {
            auto size = std::min<std::size_t>(200, fbs.end() - iter);
            ASSERT_TRUE(queue.BatchPush(&*iter, &*(iter + size), false));
            iter += size;
          }
        } else {
          latch.count_down();
          latch.wait();
          for (int i = 0; i != N / (T / 2); ++i) {
            ASSERT_TRUE(queue.Push(CreateEntity(s + i + 1), false));
          }
        }
      });
    }
    for (int i = 0; i != T / 2; ++i) {
      ts[i + T / 2] = std::thread([&] {
        std::vector<FiberEntity*> vfes;
        latch.count_down();
        latch.wait();
        while (read != N) {
          if (auto rc = queue.Pop()) {
            vfes.push_back(rc);
            ++read;
          }
        }
        std::scoped_lock lk(lock);
        for (auto&& e : vfes) {
          rcs.push_back(e);
        }
      });
    }
    for (auto&& e : ts) {
      e.join();
    }
    std::sort(rcs.begin(), rcs.end());
    ASSERT_EQ(rcs.end(), std::unique(rcs.begin(), rcs.end()));
    ASSERT_EQ(N, rcs.size());
    ASSERT_EQ(rcs.front(), CreateEntity(1));
    ASSERT_EQ(rcs.back(), CreateEntity(N));
  }
}

TEST(RunQueue, Overrun) {
  constexpr auto T = 40;
  constexpr auto N = 100'000;

  // Loop for several rounds so that we can test the case when queue's internal
  // ring buffer wraps around.
  for (int k = 0; k != 10; ++k) {
    RunQueue queue(8192);
    std::atomic<std::size_t> overruns{};
    std::atomic<std::size_t> popped{};

    std::thread ts[T], ts2[T];

    for (int i = 0; i != T; ++i) {
      ts[i] = std::thread([&overruns, &queue] {
        auto as_batch = Random() % 2 == 0;
        if (as_batch) {
          static_assert(N % 100 == 0);
          constexpr auto B = N / 100;
          std::vector<FiberEntity*> batch(B, CreateEntity(1));
          for (int j = 0; j != N; j += B) {
            while (!queue.BatchPush(&batch[0], &batch[B], false)) {
              ++overruns;
            }
          }
        } else {
          for (int j = 0; j != N; ++j) {
            while (!queue.Push(CreateEntity(1), false)) {
              ++overruns;
            }
          }
        }
      });
    }

    for (int i = 0; i != T; ++i) {
      ts2[i] = std::thread([&] {
        std::this_thread::sleep_for(1s);  // Let the queue overrun.
        while (popped.load(std::memory_order_relaxed) != N * T) {
          if (queue.Pop()) {
            ++popped;
          } else {
            std::this_thread::sleep_for(1us);
          }
        }
      });
    }
    for (auto&& e : ts) {
      e.join();
    }
    for (auto&& e : ts2) {
      e.join();
    }
    std::cout << "Overruns: " << overruns.load() << std::endl;
    ASSERT_GT(overruns.load(), 0);
    ASSERT_EQ(N * T, popped.load());
  }
}

TEST(RunQueue, Throughput) {
  constexpr auto N = 1'000'000;

  RunQueue queue(1048576);

  // Loop for several rounds so that we can test the case when queue's internal
  // ring buffer wraps around.
  for (int k = 0; k != 10; ++k) {
    constexpr auto T = 200;
    std::thread ts[T];
    Latch latch(T), latch2(T);
    std::mutex lock;
    std::vector<FiberEntity*> rcs;
    static_assert(N % T == 0);

    // Batch produce.
    for (int i = 0; i != T; ++i) {
      ts[i] = std::thread([s = N / T * i, &queue, &latch] {
        auto as_batch = Random() % 2 == 0;
        if (as_batch) {
          std::vector<FiberEntity*> fbs;
          for (int i = 0; i != N / T; ++i) {
            fbs.push_back(CreateEntity(s + i + 1));
          }
          latch.count_down();
          latch.wait();
          for (auto iter = fbs.begin(); iter < fbs.end();) {
            auto size = std::min<std::size_t>(200, fbs.end() - iter);
            ASSERT_TRUE(queue.BatchPush(&*iter, &*(iter + size), false));
            iter += size;
          }
        } else {
          latch.count_down();
          latch.wait();
          for (int i = 0; i != N / T; ++i) {
            ASSERT_TRUE(queue.Push(CreateEntity(s + i + 1), false));
          }
        }
      });
    }
    for (auto&& e : ts) {
      e.join();
    }

    // Batch consume.
    for (int i = 0; i != T; ++i) {
      ts[i] = std::thread([&] {
        std::vector<FiberEntity*> vfes;
        latch2.count_down();
        latch2.wait();
        for (int j = 0; j != N / T; ++j) {
          auto rc = queue.Pop();
          vfes.push_back(rc);
        }
        std::scoped_lock lk(lock);
        for (auto&& e : vfes) {
          rcs.push_back(e);
        }
      });
    }
    for (auto&& e : ts) {
      e.join();
    }
    std::sort(rcs.begin(), rcs.end());
    ASSERT_EQ(rcs.end(), std::unique(rcs.begin(), rcs.end()));
    ASSERT_EQ(N, rcs.size());
    ASSERT_EQ(rcs.front(), CreateEntity(1));
    ASSERT_EQ(rcs.back(), CreateEntity(N));
  }
}

}  // namespace flare::fiber::detail
