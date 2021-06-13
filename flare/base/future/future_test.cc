// Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "flare/base/future.h"

#include <condition_variable>
#include <mutex>
#include <numeric>
#include <string>
#include <thread>

#include "gtest/gtest.h"

#include "flare/base/callback.h"

using namespace std::literals;

namespace flare::future {

using MoveOnlyType = std::unique_ptr<int>;

static_assert(!std::is_copy_constructible_v<Future<>>);
static_assert(!std::is_copy_assignable_v<Future<>>);
static_assert(!std::is_copy_constructible_v<Future<int, double>>);
static_assert(!std::is_copy_assignable_v<Future<int, double>>);
static_assert(std::is_move_constructible_v<Future<>>);
static_assert(std::is_move_assignable_v<Future<>>);
static_assert(std::is_move_constructible_v<Future<int, double>>);
static_assert(std::is_move_assignable_v<Future<int, double>>);

static_assert(!std::is_copy_constructible_v<Promise<>>);
static_assert(!std::is_copy_assignable_v<Promise<>>);
static_assert(!std::is_copy_constructible_v<Promise<int, double>>);
static_assert(!std::is_copy_assignable_v<Promise<int, double>>);
static_assert(std::is_move_constructible_v<Promise<>>);
static_assert(std::is_move_assignable_v<Promise<>>);
static_assert(std::is_move_constructible_v<Promise<int, double>>);
static_assert(std::is_move_assignable_v<Promise<int, double>>);

static_assert(!std::is_copy_constructible_v<Boxed<MoveOnlyType>>);
static_assert(!std::is_copy_assignable_v<Boxed<MoveOnlyType>>);
static_assert(std::is_move_constructible_v<Boxed<MoveOnlyType>>);
static_assert(std::is_move_assignable_v<Boxed<MoveOnlyType>>);

template <class T>
using resource_ptr = std::unique_ptr<T, void (*)(T*)>;

Future<resource_ptr<void>, int> AcquireXxxAsync() {
  Promise<resource_ptr<void>, int> p;
  auto rf = p.GetFuture();

  std::thread([p = std::move(p)]() mutable {
    std::this_thread::sleep_for(10ms);
    p.SetValue(resource_ptr<void>(reinterpret_cast<void*>(1), [](auto) {}), 0);
  }).detach();

  return rf;
}

// Not tests, indeed. (Or it might be treated as a compilation test?)
TEST(Usage, Initiialization) {
  Future<> uf1;                 // Uninitialized future.
  Future<int, double> uf2;      // Uninitialized future.
  Future<> f(futurize_values);  // Ready future.
  Future<int> fi(10);  // Future with single type can be constructed directly.
  Future<int, double> fid(futurize_values, 1, 2.0);
  Future<double, float> f2{std::move(fid)};
  auto df = Future(futurize_values, 1, 2);  // Using deduction guides.
  auto vf = MakeReadyFuture();
  auto mf = MakeReadyFuture(1, 2.0);  // Future<int, double>

  ASSERT_EQ(10, BlockingGet(std::move(fi)));
  // Passing pointer to `Future` to `BlockingGet` also works.
  ASSERT_EQ(2.0, std::get<1>(BlockingGet(&f2)));
  ASSERT_EQ(2.0, std::get<1>(BlockingGet(&mf)));
}

TEST(Usage, Continuation) {
  Future<int, std::uint64_t> f(futurize_values, 1, 2);
  bool cont_called = false;

  // Values in `Future` are passed separately to the continuation.
  std::move(f).Then([&](int x, double f) {
    ASSERT_EQ(1, x);
    ASSERT_EQ(2.0, f);
    cont_called = true;
  });

  ASSERT_TRUE(cont_called);
}

TEST(Usage, ContinuationVariadic) {
  bool cont_called = false;

  // Generic lambda are also allowed.
  Future(futurize_values, 1, 2.0).Then([&](auto&&... values) {
    ASSERT_EQ(3, (... + values));
    cont_called = true;
  });

  ASSERT_TRUE(cont_called);
}

TEST(Usage, ContinuationAsyncFile) {
  Future<FILE*, int> failure_file(futurize_values, nullptr, -1);
  bool cont_called = false;

  std::move(failure_file).Then([&](FILE* fp, int ec) {  // `auto` also works.
    ASSERT_EQ(nullptr, fp);
    ASSERT_EQ(-1, ec);

    cont_called = true;
  });

  ASSERT_TRUE(cont_called);
}

TEST(Usage, ContinuationWhenAllVariadic) {
  static FILE* const kDummyFile = reinterpret_cast<FILE*>(0x10000123);

  Future<FILE*, int> async_file(futurize_values, kDummyFile, 0);
  Future<FILE*, int> failure_file(futurize_values, nullptr, -1);
  Future<resource_ptr<FILE>, int> move_only_file{
      futurize_values, resource_ptr<FILE>{nullptr, nullptr}, -2};
  Future<> void_op(futurize_values);
  bool cont_called = false;

  // Or `WhenAll(&async_file, &failure_file, &void_op, &move_only_file)`.
  WhenAll(std::move(async_file), std::move(failure_file), std::move(void_op),
          std::move(move_only_file))
      .Then([&](std::tuple<FILE*, int> af, std::tuple<FILE*, int> ff,
                std::tuple<resource_ptr<FILE>, int> mof) {
        auto&& [fp1, ec1] = af;
        auto&& [fp2, ec2] = ff;
        auto&& [fp3, ec3] = mof;

        ASSERT_EQ(kDummyFile, fp1);
        ASSERT_EQ(0, ec1);
        ASSERT_EQ(nullptr, fp2);
        ASSERT_EQ(-1, ec2);
        ASSERT_EQ(nullptr, fp3);
        ASSERT_EQ(-2, ec3);

        cont_called = true;
      });

  ASSERT_TRUE(cont_called);
}

TEST(Usage, ContinuationWhenAllVariadicOnRvalueRefs) {
  bool cont_called = false;

  BlockingGet(WhenAll(AcquireXxxAsync(), AcquireXxxAsync())
                  .Then([&](std::tuple<resource_ptr<void>, int> a,
                            std::tuple<resource_ptr<void>, int> b) {
                    auto&& [a1, a2] = a;
                    auto&& [b1, b2] = b;

                    ASSERT_NE(nullptr, a1);
                    ASSERT_EQ(0, a2);
                    ASSERT_NE(nullptr, b1);
                    ASSERT_EQ(0, b2);

                    cont_called = true;
                  }));

  ASSERT_TRUE(cont_called);
}

TEST(Usage, ContinuationWhenAllCollectionOfEmptyFuture) {
  std::vector<Future<>> vfs;
  bool cont_called = false;

  for (int i = 0; i != 1000; ++i) {
    vfs.emplace_back(futurize_values);
  }

  // std::vector<Future<>> is special, the continuation is called with
  // no argument.
  WhenAll(std::move(vfs)).Then([&] { cont_called = true; });
  ASSERT_TRUE(cont_called);
}

TEST(Usage, ContinuationWhenAnyCollectionOfEmptyFuture) {
  std::vector<Future<>> vfs;
  bool cont_called = false;

  for (int i = 0; i != 1000; ++i) {
    vfs.emplace_back(futurize_values);
  }

  WhenAny(&vfs).Then([&](std::size_t index) { cont_called = true; });
  ASSERT_TRUE(cont_called);
}

TEST(Usage, ContinuationWhenAnyCollection) {
  std::vector<Future<int>> vfs;
  bool cont_called = false;

  for (int i = 0; i != 1000; ++i) {
    vfs.emplace_back(i);
  }

  WhenAny(&vfs).Then([&](std::size_t index, int v) { cont_called = true; });
  ASSERT_TRUE(cont_called);
}

TEST(Usage, ContinuationWhenAllCollection) {
  std::vector<Future<int>> vfs;
  bool cont_called = false;

  for (int i = 0; i != 1000; ++i) {
    vfs.emplace_back(1);
  }

  // Or `WhenAll(&vfs)`.
  WhenAll(std::move(vfs)).Then([&](std::vector<int> v) {
    ASSERT_EQ(1000, std::accumulate(v.begin(), v.end(), 0));
    cont_called = true;
  });
  ASSERT_TRUE(cont_called);
}

TEST(Usage, Fork) {
  Future<int> rf(1);
  auto forked = Fork(&rf);  // (Will be) satisfied with the same value as
                            // of `rf`.
  bool cont_called = false;

  WhenAll(&rf, &forked).Then([&](int x, int y) {
    ASSERT_EQ(1, x);
    ASSERT_EQ(1, y);

    cont_called = true;
  });

  ASSERT_TRUE(cont_called);
}

TEST(Usage, ForkVoid) {
  auto rf = MakeReadyFuture();
  auto forked = Fork(&rf);
  bool cont_called = false;

  WhenAll(&rf, &forked).Then([&] { cont_called = true; });

  ASSERT_TRUE(cont_called);
}

TEST(Usage, Split) {
  {
    auto&& [f1, f2] = Split(Future<int>(1));
    bool cont_called = false;

    WhenAll(&f1, &f2).Then([&](int x, int y) {
      ASSERT_EQ(1, x);
      ASSERT_EQ(1, y);

      cont_called = true;
    });

    ASSERT_TRUE(cont_called);
  }
  {
    auto&& [f1, f2] = Split(MakeReadyFuture());
    bool cont_called = false;

    WhenAll(&f1, &f2).Then([&] { cont_called = true; });
    ASSERT_TRUE(cont_called);
  }
}

struct Latch {
  void Countdown() {
    std::unique_lock lk(m);

    if (--left) {
      cv.wait(lk, [&] { return left == 0; });
    } else {
      cv.notify_all();
    }
  }

  explicit Latch(std::size_t size) : left(size) {}
  std::mutex m;
  std::size_t left;
  std::condition_variable cv;
};

struct NonDefaultConstructible {
  explicit NonDefaultConstructible(int x) {}
  NonDefaultConstructible() = delete;
};

TEST(FutureV2Test, ReadyFuture) {
  int x = 0;
  auto ready = Future(10);

  ASSERT_EQ(0, x);
  std::move(ready).Then([&](auto xx) { x = xx; });
  ASSERT_EQ(10, x);
}

TEST(FutureV2Test, ConversionTest) {
  Future<int> f(1);
  Future<std::uint64_t> f2 = std::move(f);

  ASSERT_EQ(1, BlockingGet(std::move(f2)));
}

// Primarily a compilation test.
TEST(FutureV2Test, NonDefaultConstructibleTypes) {
  Promise<NonDefaultConstructible> p;

  p.SetValue(10);
}

TEST(FutureV2Test, MoveOnlyWhenAllVariadic) {
  bool f = false;
  Promise<std::unique_ptr<int>, std::unique_ptr<char>> p1;
  Promise<> p2;

  WhenAll(p1.GetFuture(), p2.GetFuture()).Then([&](auto p) {
    auto&& [pi, pc] = p;
    ASSERT_NE(nullptr, pi);
    ASSERT_EQ(nullptr, pc);

    f = true;
  });

  p1.SetValue(std::make_unique<int>(), std::unique_ptr<char>{});
  ASSERT_FALSE(f);
  p2.SetValue();
  ASSERT_TRUE(f);
}

TEST(FutureV2Test, MoveOnlyWhenAllCollection) {
  constexpr auto kCount = 10000;
  std::vector<Promise<std::unique_ptr<int>, NonDefaultConstructible>> vps(
      kCount);
  std::vector<Future<>> vfs;
  int x = 0;

  for (auto&& e : vps) {
    vfs.emplace_back(e.GetFuture().Then([&](auto&&...) { ++x; }));
  }

  auto rc = WhenAll(std::move(vfs));
  ASSERT_EQ(0, x);

  for (auto&& e : vps) {
    e.SetValue(std::make_unique<int>(), 10);
  }

  ASSERT_EQ(kCount, x);
  BlockingGet(std::move(rc));  // Not needed, though.
  ASSERT_EQ(kCount, x);
}

TEST(FutureV2Test, MoveOnlyBlockingGet) {
  for (int i = 0; i != 10000; ++i) {
    std::atomic<bool> f = false;
    Promise<std::unique_ptr<int>, std::unique_ptr<char>> p1;

    // Counterexample actually..
    //
    // The future is satisfied once `p1.SetValue` is called, potentially
    // before `f` is set to true.
    /*
    std::thread([&] () { p1.SetValue(); f = true;}).detach();
    BlockingGet(p1.GetFuture());
    */

    std::thread([&] { p1.SetValue(); }).detach();
    BlockingGet(p1.GetFuture().Then([&](auto&&...) { f = true; }));
    ASSERT_TRUE(f);
  }
}

TEST(FutureV2Test, CompatibleConversion) {
  Future<int> f(10);
  Future<std::uint64_t> f2 = std::move(f);

  ASSERT_EQ(10, BlockingGet(std::move(f2)));
}

TEST(FutureV2Test, WhenAllCollectionMultithreaded) {
  for (int i = 0; i != 100; ++i) {
    constexpr auto kCount = 100;
    std::vector<Promise<std::unique_ptr<int>, char>> vps(kCount);
    std::vector<Future<>> vfs;
    std::vector<std::thread> ts;
    Latch latch(kCount + 1);
    std::atomic<int> x{};

    for (auto&& e : vps) {
      vfs.emplace_back(e.GetFuture().Then([&](auto&&...) { ++x; }));
    }

    auto all = WhenAll(std::move(vfs));
    ASSERT_EQ(0, x);

    for (auto&& e : vps) {
      ts.push_back(std::thread([&, ep = &e] {
        latch.Countdown();
        ep->SetValue(std::make_unique<int>(), 'a');
      }));
    }
    ASSERT_EQ(0, x);

    std::thread([&] {
      std::this_thread::sleep_for(10ms);
      latch.Countdown();
    }).detach();
    BlockingGet(std::move(all));

    ASSERT_EQ(kCount, x);

    // So that `lock` won't be destroyed before the threads exit.
    for (auto&& e : ts) {
      e.join();
    }
  }
}

// std::vector<bool> is special in that accessing individual members
// might race between each other.
//
// > Notwithstanding (17.6.5.9), implementations are required to avoid
// > data races when the contents of the contained object in different
// > elements in the same sequence, excepting vector<bool>, are modified
// > concurrently.
TEST(FutureV2Test, WhenAllCollectionMultithreadedBool) {
  for (int i = 0; i != 1000; ++i) {
    constexpr auto kCount = 100;

    std::vector<Promise<bool>> vps(kCount);
    std::vector<Future<bool>> vfs;
    std::vector<std::thread> ts(kCount);
    Latch latch(kCount + 1);
    std::atomic<bool> cont_called = false;

    for (int i = 0; i != kCount; ++i) {
      vfs.emplace_back(vps[i].GetFuture());
    }

    WhenAll(&vfs).Then([&](std::vector<bool> v) {
      ASSERT_TRUE(std::all_of(v.begin(), v.end(), [&](auto x) { return x; }));
      cont_called = true;
    });

    for (int i = 0; i != kCount; ++i) {
      ts[i] = std::thread([&latch, &vps, i] {
        latch.Countdown();
        vps[i].SetValue(true);
      });
    }

    ASSERT_FALSE(cont_called);
    latch.Countdown();
    for (auto&& e : ts) {
      e.join();
    }
    ASSERT_TRUE(cont_called);
  }
}

TEST(FutureV2Test, WhenAnyCollectionMultithreaded) {
  for (int i = 0; i != 100; ++i) {
    constexpr auto kCount = 100;
    std::vector<Promise<std::unique_ptr<int>, char>> vps(kCount);
    std::vector<Future<char>> vfs;
    std::vector<std::thread> ts;
    Latch latch(kCount + 1);
    std::atomic<int> x{};

    for (auto&& e : vps) {
      vfs.emplace_back(e.GetFuture().Then([&](auto&&...) {
        ++x;
        return 'a';
      }));
    }

    auto all = WhenAny(std::move(vfs));
    ASSERT_EQ(0, x);

    for (auto&& e : vps) {
      ts.push_back(std::thread([&, ep = &e] {
        latch.Countdown();
        ep->SetValue(std::make_unique<int>(), 'a');
      }));
    }
    ASSERT_EQ(0, x);

    std::thread([&] {
      std::this_thread::sleep_for(10ms);
      latch.Countdown();
    }).detach();

    auto&& [index, value] = BlockingGet(&all);

    ASSERT_GE(kCount, index);
    ASSERT_LE(0, index);
    ASSERT_EQ('a', value);

    for (auto&& e : ts) {
      e.join();
    }
    ASSERT_EQ(kCount, x);
  }
}

TEST(FutureV2Test, WhenAllVariadicMultithreaded) {
  for (int i = 0; i != 10000; ++i) {
    bool f = false;
    Promise<std::unique_ptr<int>, std::unique_ptr<char>> p1;
    Promise<> p2;
    Latch latch(2 + 1);

    auto all = WhenAll(p1.GetFuture(), p2.GetFuture()).Then([&](auto p) {
      auto&& [pi, pc] = p;
      ASSERT_NE(nullptr, pi);
      ASSERT_EQ(nullptr, pc);

      f = true;
    });

    auto t1 = std::thread([&] {
      latch.Countdown();
      p1.SetValue(std::make_unique<int>(), std::unique_ptr<char>());
    });
    auto t2 = std::thread([&] {
      latch.Countdown();
      p2.SetValue();
    });

    ASSERT_FALSE(f);
    latch.Countdown();
    BlockingGet(std::move(all));
    ASSERT_TRUE(f);

    t1.join();
    t2.join();
  }
}

TEST(FutureV2Test, WhenAllCollectionEmpty) {
  {
    std::vector<Future<>> vfs;
    int x{};
    WhenAll(std::move(vfs)).Then([&] { x = 10; });
    ASSERT_EQ(10, x);
  }

  {
    std::vector<Future<int>> vfs;
    int x{};
    WhenAll(std::move(vfs)).Then([&](auto) { x = 10; });
    ASSERT_EQ(10, x);
  }
}

TEST(FutureV2Test, WhenAllOnCollectionOfEmptyFutures) {
  constexpr auto kCount = 100000;
  std::vector<Future<>> vfs(kCount);

  for (int i = 0; i != kCount; ++i) {
    vfs[i] = Future(futurize_values);
  }

  int x = 0;

  WhenAll(std::move(vfs)).Then([&] { x = 100; });
  ASSERT_EQ(100, x);
}

TEST(FutureV2Test, Chaining) {
  constexpr auto kLoopCount = 10000;

  Promise<> p;
  auto f = p.GetFuture();
  int c = 0;

  for (int i = 0; i != kLoopCount; ++i) {
    f = std::move(f).Then([&c] { ++c; });
  }

  ASSERT_EQ(0, c);
  p.SetValue();
  ASSERT_EQ(kLoopCount, c);
}

TEST(FutureV2Test, ConcurrentFork) {
  for (int i = 0; i != 100000; ++i) {
    Promise<std::string> ps;
    auto fs = ps.GetFuture();
    Latch l(2);
    std::atomic<int> x{};
    auto t = std::thread([&] {
      l.Countdown();
      Fork(&fs).Then([&](auto&&...) { ++x; });
    });

    l.Countdown();
    ps.SetValue("asdf");  // Will be concurrently executed with `Fork(&fs)`.
    t.join();

    ASSERT_EQ(1, x);
  }
}

TEST(FutureV2Test, DurationTimeout) {
  {
    Promise<int> p;
    auto rc = BlockingTryGet(p.GetFuture(), 1s);
    ASSERT_FALSE(rc);
    p.SetValue(10);
  }
  {
    Promise<int> p;
    auto f = p.GetFuture();
    auto rc = BlockingTryGet(&f, 1s);
    ASSERT_FALSE(rc);
  }
  {
    Promise<> p;
    auto f = p.GetFuture();
    auto rc = BlockingTryGet(&f, 1s);
    ASSERT_FALSE(rc);
  }
}

TEST(FutureV2Test, DurationTimePoint) {
  {
    Promise<int> p;
    auto rc =
        BlockingTryGet(p.GetFuture(), std::chrono::system_clock::now() + 1s);
    ASSERT_FALSE(rc);
  }
  {
    Promise<int> p;
    auto f = p.GetFuture();
    auto rc = BlockingTryGet(&f, std::chrono::system_clock::now() + 1s);
    ASSERT_FALSE(rc);
  }
  {
    Promise<> p;
    auto f = p.GetFuture();
    auto rc = BlockingTryGet(&f, std::chrono::system_clock::now() + 1s);
    ASSERT_FALSE(rc);
  }
}

TEST(FutureV2Test, Repeat) {
  int ct = 0;
  bool f = false;

  Repeat([&] { return ++ct != 100; }).Then([&] { f = true; });

  ASSERT_EQ(100, ct);
  ASSERT_TRUE(f);
}

TEST(FutureV2Test, RepeatIfReturnsVoid) {
  std::vector<int> v;
  int ct = 0;
  bool f = false;

  RepeatIf([&] { v.push_back(++ct); }, [&] { return v.size() < 100; })
      .Then([&] { f = true; });

  ASSERT_EQ(100, ct);
  ASSERT_EQ(100, v.size());
}

TEST(FutureV2Test, RepeatIfReturnsValue) {
  std::vector<int> v;
  int ct = 0;
  bool f = false;

  RepeatIf(
      [&] {
        v.push_back(++ct);
        return std::make_unique<int>(v.size());  // Move only.
      },
      [&](auto&& s) { return *s < 100; }  // Can NOT pass-by-value.
      )
      .Then([&](auto s) {
        ASSERT_EQ(100, *s);
        f = true;
      });

  ASSERT_EQ(100, ct);
  ASSERT_EQ(100, v.size());

  for (int i = 0; i != v.size(); ++i) {
    ASSERT_EQ(i + 1, v[i]);
  }
}

TEST(FutureV2Test, RepeatIfReturnsMultipleValue) {
  std::vector<int> v;
  int ct = 0;

  auto&& [vv, s] = BlockingGet(RepeatIf(
      [&] {
        v.push_back(++ct);
        return Future(futurize_values, 10,
                      std::make_unique<int>(v.size()));  // Move only.
      },
      [&](int v, auto&& s) { return *s < 100; }));  // Can NOT pass-by-value.

  ASSERT_EQ(10, vv);
  ASSERT_EQ(100, *s);
  ASSERT_EQ(100, ct);
  ASSERT_EQ(100, v.size());

  for (int i = 0; i != v.size(); ++i) {
    ASSERT_EQ(i + 1, v[i]);
  }
}

std::atomic<std::uint64_t> posted_jobs = 0;

class FancyExecutor {
 public:
  void Execute(Function<void()> job) {
    ++posted_jobs;
    std::thread([job = std::move(job)] { job(); }).detach();
  }
};

TEST(FutureV2Test, ExecutorTest) {
  ASSERT_EQ(0, posted_jobs);

  {
    Promise<> p;
    p.GetFuture().Then([] {});
    p.SetValue();

    ASSERT_EQ(0, posted_jobs);
  }

  // Now we enable executor.
  FancyExecutor fe;
  SetDefaultExecutor(fe);
  posted_jobs = 0;

  auto test = [] {
    std::mutex m;
    std::condition_variable cv;
    std::atomic<bool> last_one = false;

    Promise<> p;
    auto f = p.GetFuture();

    // Now we won't overflow the stack even if we make a very long chain.
    for (int i = 0; i != 10000; ++i) {
      f = std::move(f).Then([&] { ASSERT_GT(posted_jobs, 0); });
    }
    p.SetValue();

    std::move(f).Then([&] {
      // The lock is required so that a spurious wake up of `cv` between change
      // of `last_one` and notifying `cv` won't cause `cv.wait` below to pass
      // and destroy `cv` (as a consequence of leaving the scope).
      //
      // This issue was reported by tsan as a race between `pthread_cond_notify`
      // and `pthread_cond_destory`, although I think this is a separate bug in
      // tsan as it's clearly stated that destorying a condition-variable once
      // all threads waiting on it are awakened is well-defined.
      //
      // [https://linux.die.net/man/3/pthread_cond_destroy]
      //
      // > A condition variable can be destroyed immediately after all the
      // > threads that are blocked on it are awakened.
      //
      // Nonetheless this is still a race in our code, so we lock on `m` here.
      std::lock_guard lk(m);
      last_one = true;
      cv.notify_one();
    });

    std::unique_lock lk(m);
    cv.wait(lk, [&] { return last_one.load(); });

    ASSERT_GT(posted_jobs, 0);
  };

  std::vector<std::thread> vt(10);
  for (auto&& t : vt) {
    t = std::thread(test);
  }
  for (auto&& t : vt) {
    t.join();
  }

  ASSERT_EQ(posted_jobs, 10 * 10000 + 10);

  // Use the default executor.
  SetDefaultExecutor(InlineExecutor());

  {
    posted_jobs = 0;

    Promise<> p;
    p.GetFuture().Then([] {});
    p.SetValue();

    ASSERT_EQ(0, posted_jobs);
  }
}

TEST(FutureV2DeathTest, WhenAnyCollectionEmpty) {
  std::vector<Future<>> vfs;
  ASSERT_DEATH(WhenAny(std::move(vfs)), "on an empty collection is undefined");

  std::vector<Future<int>> vfs2;
  ASSERT_DEATH(WhenAny(&vfs2), "on an empty collection is undefined");
}

TEST(FutureDeathTest, DeathOnException) {
  ASSERT_DEATH(
      Future(1).Then([](int) { throw std::runtime_error("Fancy death"); }),
      "Fancy death");
}

}  // namespace flare::future

// UT below won't compile. ADL shouldn't kick in.
//
// namespace non_flare {
//
// TEST(Future, ADL) {
//   flare::Future<> f;
//   BlockingGet(std::move(f));
// }
//
// }  // namespace non_flare
