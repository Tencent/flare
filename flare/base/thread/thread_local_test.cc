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

#include "flare/base/thread/thread_local.h"

#include <dlfcn.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <atomic>
#include <chrono>
#include <climits>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#include "googletest/gtest/gtest.h"

#include "flare/base/thread/latch.h"

namespace flare {

struct Widget {
  static int total;
  int val_;
  ~Widget() { total += val_; }
};

int Widget::total = 0;

TEST(ThreadLocal, BasicDestructor) {
  Widget::total = 0;
  ThreadLocal<Widget> w;
  std::thread([&w]() { w->val_ += 10; }).join();
  EXPECT_EQ(10, Widget::total);
}

TEST(ThreadLocal, SimpleRepeatDestructor) {
  Widget::total = 0;
  {
    ThreadLocal<Widget> w;
    w->val_ += 10;
  }
  {
    ThreadLocal<Widget> w;
    w->val_ += 10;
  }
  EXPECT_EQ(20, Widget::total);
}

TEST(ThreadLocal, InterleavedDestructors) {
  Widget::total = 0;
  std::unique_ptr<ThreadLocal<Widget>> w;
  int version = 0;
  const int version_max = 2;
  int th_iter = 0;
  std::mutex lock;
  auto th = std::thread([&]() {
    int version_prev = 0;
    while (true) {
      while (true) {
        std::lock_guard<std::mutex> g(lock);
        if (version > version_max) {
          return;
        }
        if (version > version_prev) {
          // We have a new version of w, so it should be initialized to zero
          EXPECT_EQ(0, (*w)->val_);
          break;
        }
      }
      std::lock_guard<std::mutex> g(lock);
      version_prev = version;
      (*w)->val_ += 10;
      ++th_iter;
    }
  });
  for (size_t i = 0; i < version_max; ++i) {
    int th_iter_prev = 0;
    {
      std::lock_guard<std::mutex> g(lock);
      th_iter_prev = th_iter;
      w = std::make_unique<ThreadLocal<Widget>>();
      ++version;
    }
    while (true) {
      std::lock_guard<std::mutex> g(lock);
      if (th_iter > th_iter_prev) {
        break;
      }
    }
  }
  {
    std::lock_guard<std::mutex> g(lock);
    version = version_max + 1;
  }
  th.join();
  EXPECT_EQ(version_max * 10, Widget::total);
}

class SimpleThreadCachedInt {
  ThreadLocal<int> val_;

 public:
  void add(int val) { *val_ += val; }

  int read() {
    int ret = 0;
    val_.ForEach([&](const int* p) { ret += *p; });
    return ret;
  }
};

TEST(ThreadLocal, AccessAllThreadsCounter) {
  const int kNumThreads = 256;
  SimpleThreadCachedInt stci[kNumThreads + 1];
  std::atomic<bool> run(true);
  std::atomic<int> total_atomic{0};
  std::vector<std::thread> threads;
  // thread i will increment all the thread locals
  // in the range 0..i
  for (int i = 0; i < kNumThreads; ++i) {
    threads.push_back(std::thread([i, &stci, &run, &total_atomic]() {
      for (int j = 0; j <= i; ++j) {
        stci[j].add(1);
      }

      total_atomic.fetch_add(1);
      while (run.load()) {
        usleep(100);
      }
    }));
  }
  while (total_atomic.load() != kNumThreads) {
    usleep(100);
  }
  for (int i = 0; i <= kNumThreads; ++i) {
    EXPECT_EQ(kNumThreads - i, stci[i].read());
  }
  run.store(false);
  for (auto& t : threads) {
    t.join();
  }
}

TEST(ThreadLocal, resetNull) {
  ThreadLocal<int> tl;
  tl.Reset(std::make_unique<int>(4));
  EXPECT_EQ(4, *tl.Get());
  tl.Reset();
  EXPECT_EQ(0, *tl.Get());
  tl.Reset(std::make_unique<int>(5));
  EXPECT_EQ(5, *tl.Get());
}

struct Foo {
  ThreadLocal<int> tl;
};

TEST(ThreadLocal, Movable1) {
  Foo a;
  Foo b;
  EXPECT_TRUE(a.tl.Get() != b.tl.Get());
}

TEST(ThreadLocal, Movable2) {
  std::map<int, Foo> map;

  map[42];
  map[10];
  map[23];
  map[100];

  std::set<void*> tls;
  for (auto& m : map) {
    tls.insert(m.second.tl.Get());
  }

  // Make sure that we have 4 different instances of *tl
  EXPECT_EQ(4, tls.size());
}

using TLPInt = ThreadLocal<std::atomic<int>>;

template <typename Op, typename Check>
void StressAccessTest(Op op, Check check, size_t num_threads,
                      size_t num_loops) {
  TLPInt ptr;
  ptr.Reset(std::make_unique<std::atomic<int>>(0));
  std::atomic<bool> running{true};

  Latch l(num_threads + 1);

  std::vector<std::thread> threads;

  for (size_t k = 0; k < num_threads; ++k) {
    threads.emplace_back([&] {
      ptr.Reset(std::make_unique<std::atomic<int>>(1));

      l.count_down();
      l.wait();

      while (running.load()) {
        op(ptr);
      }
    });
  }

  // wait for the threads to be up and running
  l.count_down();
  l.wait();

  for (size_t n = 0; n < num_loops; ++n) {
    int sum = 0;
    ptr.ForEach([&](const std::atomic<int>* p) { sum += *p; });
    check(sum, num_threads);
  }

  running.store(false);
  for (auto& t : threads) {
    t.join();
  }
}

TEST(ThreadLocal, StressAccessReset) {
  StressAccessTest(
      [](TLPInt& ptr) { ptr.Reset(std::make_unique<std::atomic<int>>(1)); },
      [](size_t sum, size_t num_threads) { EXPECT_EQ(sum, num_threads); }, 16,
      10);
}

TEST(ThreadLocal, StressAccessSet) {
  StressAccessTest(
      [](TLPInt& ptr) { *ptr = 1; },
      [](size_t sum, size_t num_threads) { EXPECT_EQ(sum, num_threads); }, 16,
      100);
}

TEST(ThreadLocal, StressAccessRelease) {
  StressAccessTest(
      [](TLPInt& ptr) {
        auto* p = ptr.Leak();
        delete p;
        ptr.Reset(std::make_unique<std::atomic<int>>(1));
      },
      [](size_t sum, size_t num_threads) { EXPECT_LE(sum, num_threads); }, 8,
      4);
}

}  // namespace flare
