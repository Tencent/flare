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

#include "flare/fiber/detail/waitable.h"

#include <atomic>
#include <deque>
#include <memory>
#include <numeric>
#include <queue>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

#include "flare/base/chrono.h"
#include "flare/base/random.h"
#include "flare/base/tsc.h"
#include "flare/fiber/detail/fiber_entity.h"
#include "flare/fiber/detail/fiber_worker.h"
#include "flare/fiber/detail/scheduling_group.h"
#include "flare/fiber/detail/testing.h"
#include "flare/fiber/detail/timer_worker.h"

using namespace std::literals;

namespace flare::fiber::detail {

namespace {

void Sleep(std::chrono::nanoseconds ns) {
  WaitableTimer wt(ReadSteadyClock() + ns);
  wt.wait();
}

void RandomDelay() {
  auto round = Random(100);
  for (int i = 0; i != round; ++i) {
    ReadTsc();
  }
}

}  // namespace

class SystemFiberOrNot : public ::testing::TestWithParam<bool> {};

// **Concurrently** call `cb` for `times`.
template <class F>
void RunInFiber(std::size_t times, bool system_fiber, F cb) {
  std::atomic<std::size_t> called{};
  auto sg = std::make_unique<SchedulingGroup>(std::vector<int>{}, 16);
  TimerWorker timer_worker(sg.get());
  sg->SetTimerWorker(&timer_worker);
  std::deque<FiberWorker> workers;

  for (int i = 0; i != 16; ++i) {
    workers.emplace_back(sg.get(), i).Start(false);
  }
  timer_worker.Start();
  for (int i = 0; i != times; ++i) {
    testing::StartFiberEntityInGroup(sg.get(), system_fiber, [&, i] {
      cb(i);
      ++called;
    });
  }
  while (called != times) {
    std::this_thread::sleep_for(100ms);
  }
  sg->Stop();
  timer_worker.Stop();
  for (auto&& w : workers) {
    w.Join();
  }
  timer_worker.Join();
}

TEST_P(SystemFiberOrNot, WaitableTimer) {
  RunInFiber(100, GetParam(), [](auto) {
    auto now = ReadSteadyClock();
    WaitableTimer wt(now + 1s);
    wt.wait();
    EXPECT_NEAR(1s / 1ms, (ReadSteadyClock() - now) / 1ms, 100);
  });
}

TEST_P(SystemFiberOrNot, Mutex) {
  for (int i = 0; i != 10; ++i) {
    Mutex m;
    int value = 0;
    RunInFiber(10000, GetParam(), [&](auto) {
      std::scoped_lock _(m);
      ++value;
    });
    ASSERT_EQ(10000, value);
  }
}

TEST_P(SystemFiberOrNot, ConditionVariable) {
  constexpr auto N = 10000;

  for (int i = 0; i != 10; ++i) {
    Mutex m[N];
    ConditionVariable cv[N];
    std::queue<int> queue[N];
    std::atomic<std::size_t> read{}, write{};
    int sum[N] = {};

    // We, in fact, are passing data between two scheduling groups.
    //
    // This should work anyway.
    auto prods = std::thread([&] {
      RunInFiber(N, GetParam(), [&](auto index) {
        auto to = Random(N - 1);
        std::scoped_lock _(m[to]);
        queue[to].push(index);
        cv[to].notify_one();
        ++write;
      });
    });
    auto signalers = std::thread([&] {
      RunInFiber(N, GetParam(), [&](auto index) {
        std::unique_lock lk(m[index]);
        bool exit = false;
        while (!exit) {
          auto&& mq = queue[index];
          cv[index].wait(lk, [&] { return !mq.empty(); });
          ASSERT_TRUE(lk.owns_lock());
          while (!mq.empty()) {
            if (mq.front() == -1) {
              exit = true;
              break;
            }
            sum[index] += mq.front();
            ++read;
            mq.pop();
          }
        }
      });
    });
    prods.join();
    RunInFiber(N, GetParam(), [&](auto index) {
      std::scoped_lock _(m[index]);
      queue[index].push(-1);
      cv[index].notify_one();
    });
    signalers.join();
    ASSERT_EQ((N - 1) * N / 2,
              std::accumulate(std::begin(sum), std::end(sum), 0));
  }
}

TEST_P(SystemFiberOrNot, ConditionVariable2) {
  constexpr auto N = 1000;

  for (int i = 0; i != 50; ++i) {
    Mutex m[N];
    ConditionVariable cv[N];
    bool f[N] = {};
    std::atomic<int> sum{};
    auto prods = std::thread([&] {
      RunInFiber(N, GetParam(), [&](auto index) {
        Sleep(Random(10) * 1ms);
        std::scoped_lock _(m[index]);
        f[index] = true;
        cv[index].notify_one();
      });
    });
    auto signalers = std::thread([&] {
      RunInFiber(N, GetParam(), [&](auto index) {
        Sleep(Random(10) * 1ms);
        std::unique_lock lk(m[index]);
        cv[index].wait(lk, [&] { return f[index]; });
        ASSERT_TRUE(lk.owns_lock());
        sum += index;
      });
    });
    prods.join();
    signalers.join();
    ASSERT_EQ((N - 1) * N / 2, sum);
  }
}

TEST_P(SystemFiberOrNot, ConditionVariableNoTimeout) {
  constexpr auto N = 1000;
  std::atomic<std::size_t> done{};
  Mutex m;
  ConditionVariable cv;
  auto waiter = std::thread([&] {
    RunInFiber(N, GetParam(), [&](auto index) {
      std::unique_lock lk(m);
      done += cv.wait_until(lk, ReadSteadyClock() + 100s);
    });
  });
  std::thread([&] {
    RunInFiber(1, GetParam(), [&](auto index) {
      while (done != N) {
        cv.notify_all();
      }
    });
  }).join();
  ASSERT_EQ(N, done);
  waiter.join();
}

TEST_P(SystemFiberOrNot, ConditionVariableTimeout) {
  constexpr auto N = 1000;
  std::atomic<std::size_t> timed_out{};
  Mutex m;
  ConditionVariable cv;
  RunInFiber(N, GetParam(), [&](auto index) {
    std::unique_lock lk(m);
    timed_out += !cv.wait_until(lk, ReadSteadyClock() + 1ms);
  });
  ASSERT_EQ(N, timed_out);
}

TEST_P(SystemFiberOrNot, ConditionVariableRace) {
  constexpr auto N = 1000;

  for (int i = 0; i != 5; ++i) {
    Mutex m;
    ConditionVariable cv;
    std::atomic<int> sum{};
    auto prods = std::thread([&] {
      RunInFiber(N, GetParam(), [&](auto index) {
        for (int j = 0; j != 100; ++j) {
          Sleep(Random(100) * 1us);
          std::scoped_lock _(m);
          cv.notify_all();
        }
      });
    });
    auto signalers = std::thread([&] {
      RunInFiber(N, GetParam(), [&](auto index) {
        for (int j = 0; j != 100; ++j) {
          std::unique_lock lk(m);
          cv.wait_until(lk, ReadSteadyClock() + 50us);
          ASSERT_TRUE(lk.owns_lock());
        }
        sum += index;
      });
    });
    prods.join();
    signalers.join();
    ASSERT_EQ((N - 1) * N / 2, sum);
  }
}

TEST_P(SystemFiberOrNot, ExitBarrier) {
  constexpr auto N = 10000;

  for (int i = 0; i != 10; ++i) {
    std::deque<ExitBarrier> l;
    std::atomic<std::size_t> waited{};

    for (int j = 0; j != N; ++j) {
      l.emplace_back();
    }

    auto counters = std::thread([&] {
      RunInFiber(N, GetParam(), [&](auto index) {
        Sleep(Random(10) * 1ms);
        l[index].UnsafeCountDown(l[index].GrabLock());
      });
    });
    auto waiters = std::thread([&] {
      RunInFiber(N, GetParam(), [&](auto index) {
        Sleep(Random(10) * 1ms);
        l[index].Wait();
        ++waited;
      });
    });
    counters.join();
    waiters.join();
    ASSERT_EQ(N, waited);
  }
}

TEST_P(SystemFiberOrNot, Event) {
  constexpr auto N = 10000;

  for (int i = 0; i != 10; ++i) {
    std::deque<Event> evs;
    std::atomic<std::size_t> waited{};

    for (int j = 0; j != N; ++j) {
      evs.emplace_back();
    }

    auto counters = std::thread([&] {
      RunInFiber(N, GetParam(), [&](auto index) {
        RandomDelay();
        evs[index].Set();
      });
    });
    auto waiters = std::thread([&] {
      RunInFiber(N, GetParam(), [&](auto index) {
        RandomDelay();
        evs[index].Wait();
        ++waited;
      });
    });
    counters.join();
    waiters.join();
    ASSERT_EQ(N, waited);
  }
}

TEST_P(SystemFiberOrNot, OneshotTimedEvent) {
  RunInFiber(1, GetParam(), [&](auto index) {
    OneshotTimedEvent ev1(ReadSteadyClock() + 1s),
        ev2(ReadSteadyClock() + 10ms);

    auto start = ReadSteadyClock();
    ev2.Wait();
    EXPECT_LT((ReadSteadyClock() - start) / 1ms, 100);

    auto t = std::thread([&] {
      std::this_thread::sleep_for(500ms);
      ev1.Set();
    });
    start = ReadSteadyClock();
    ev1.Wait();
    EXPECT_NEAR((ReadSteadyClock() - start) / 1ms, 500, 100);
    t.join();
  });
}

TEST_P(SystemFiberOrNot, OneshotTimedEventTorture) {
  constexpr auto N = 10000;

  RunInFiber(1, GetParam(), [&](auto) {
    for (int i = 0; i != 10; ++i) {
      std::deque<OneshotTimedEvent> evs;
      std::atomic<std::size_t> waited{};

      for (int j = 0; j != N; ++j) {
        evs.emplace_back(ReadSteadyClock() + Random(1000) * 1ms);
      }
      auto counters = std::thread([&] {
        RunInFiber(N, GetParam(), [&](auto index) {
          RandomDelay();
          evs[index].Set();
        });
      });
      auto waiters = std::thread([&] {
        RunInFiber(N, GetParam(), [&](auto index) {
          RandomDelay();
          evs[index].Wait();
          ++waited;
        });
      });
      counters.join();
      waiters.join();
      ASSERT_EQ(N, waited);
    }
  });
}

TEST_P(SystemFiberOrNot, EventFreeOnWakeup) {
  // This UT detects a use-after-free race, but it's can only be revealed by
  // UBSan in most cases, unfortunately.
  RunInFiber(10, GetParam(), [&](auto index) {
    std::vector<std::thread> ts(1000);
    for (int i = 0; i != 1000; ++i) {
      auto ev = std::make_unique<Event>();
      ts[i] = std::thread([&] { ev->Set(); });
      ev->Wait();
      ev = nullptr;
    }
    for (auto&& e : ts) {
      e.join();
    }
  });
}

INSTANTIATE_TEST_SUITE_P(Waitable, SystemFiberOrNot,
                         ::testing::Values(true, false));

}  // namespace flare::fiber::detail
