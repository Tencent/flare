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

#include "flare/base/ref_ptr.h"

#include <atomic>
#include <chrono>
#include <thread>

#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/base/chrono.h"
#include "flare/base/random.h"

using namespace std::literals;

namespace flare {

struct RefCounted1 {
  std::atomic<int> ref_count{1};
  RefCounted1() { ++instances; }
  ~RefCounted1() { --instances; }
  int xxx = 12345;
  inline static int instances = 0;
};

template <>
struct RefTraits<RefCounted1> {
  static void Reference(RefCounted1* rc) {
    rc->ref_count.fetch_add(1, std::memory_order_relaxed);
  }
  static void Dereference(RefCounted1* rc) {
    if (rc->ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      delete rc;
    }
  }
};

struct RefCounted2 : RefCounted<RefCounted2> {
  RefCounted2() { ++instances; }
  ~RefCounted2() { --instances; }
  inline static int instances = 0;
};

struct RefCountedVirtual : RefCounted<RefCountedVirtual> {
  RefCountedVirtual() { ++instances; }
  virtual ~RefCountedVirtual() { --instances; }
  inline static int instances = 0;
};

struct RefCounted3 : RefCountedVirtual {
  RefCounted3() { ++instances; }
  ~RefCounted3() { --instances; }
  inline static int instances = 0;
};

static_assert(!detail::is_ref_counted_directly_v<RefCounted3>);
static_assert(detail::is_ref_counted_indirectly_safe_v<RefCounted3>);

TEST(RefPtr, ReferenceCount) {
  {
    auto ptr = new RefCounted1();
    ptr->ref_count = 0;
    ASSERT_EQ(1, RefCounted1::instances);
    RefPtr p1(ref_ptr, ptr);
    ASSERT_EQ(1, ptr->ref_count);
    {
      RefPtr p2(p1);
      ASSERT_EQ(2, ptr->ref_count);
      RefPtr p3(std::move(p2));
      ASSERT_EQ(2, ptr->ref_count);
    }
    {
      RefPtr p2(p1);
      ASSERT_EQ(2, ptr->ref_count);
      p2.Reset();
      ASSERT_EQ(1, ptr->ref_count);
    }
    {
      RefPtr p2(p1);
      ASSERT_EQ(2, ptr->ref_count);
      auto ptr = p2.Leak();
      ASSERT_EQ(2, ptr->ref_count);
      RefPtr p3(adopt_ptr, ptr);
      ASSERT_EQ(2, ptr->ref_count);
    }
    ASSERT_EQ(1, ptr->ref_count);
  }
  ASSERT_EQ(0, RefCounted1::instances);
}

TEST(RefPtr, RefCounted) {
  {
    auto ptr = new RefCounted2();
    ASSERT_EQ(1, RefCounted2::instances);
    RefPtr p1(adopt_ptr, ptr);
  }
  ASSERT_EQ(0, RefCounted2::instances);
}

TEST(RefPtr, RefCountedVirtualDtor) {
  {
    auto ptr = new RefCounted3();
    ASSERT_EQ(1, RefCounted3::instances);
    ASSERT_EQ(1, RefCountedVirtual::instances);
    RefPtr p1(adopt_ptr, ptr);
  }
  ASSERT_EQ(0, RefCounted3::instances);
}

TEST(RefPtr, ImplicitlyCast) {
  {
    RefPtr ptr = MakeRefCounted<RefCounted3>();
    ASSERT_EQ(1, RefCounted3::instances);
    ASSERT_EQ(1, RefCountedVirtual::instances);
    RefPtr<RefCountedVirtual> p1(ptr);
    ASSERT_EQ(1, RefCounted3::instances);
    ASSERT_EQ(1, RefCountedVirtual::instances);
    RefPtr<RefCountedVirtual> p2(std::move(ptr));
    ASSERT_EQ(1, RefCounted3::instances);
    ASSERT_EQ(1, RefCountedVirtual::instances);
  }
  ASSERT_EQ(0, RefCounted3::instances);
  ASSERT_EQ(0, RefCountedVirtual::instances);
}

TEST(RefPtr, CopyFromNull) {
  RefPtr<RefCounted1> p1, p2;
  p1 = p2;
  // Shouldn't crash.
}

TEST(RefPtr, MoveFromNull) {
  RefPtr<RefCounted1> p1, p2;
  p1 = std::move(p2);
  // Shouldn't crash.
}

TEST(RefPtr, AtomicOps) {
  std::atomic<RefPtr<RefCounted1>> atomic{nullptr};

  EXPECT_EQ(0, RefCounted1::instances);
  EXPECT_EQ(nullptr, atomic.load());
  EXPECT_EQ(0, RefCounted1::instances);
  auto p1 = MakeRefCounted<RefCounted1>();
  EXPECT_EQ(1, RefCounted1::instances);
  atomic.store(p1);
  EXPECT_EQ(p1.Get(), atomic.load().Get());
  auto p2 = MakeRefCounted<RefCounted1>();
  EXPECT_EQ(2, RefCounted1::instances);
  EXPECT_EQ(p1.Get(), atomic.exchange(p2).Get());
  EXPECT_EQ(2, RefCounted1::instances);
  p1.Reset();
  EXPECT_EQ(1, RefCounted1::instances);
  EXPECT_FALSE(atomic.compare_exchange_strong(p1, p2));
  EXPECT_TRUE(atomic.compare_exchange_weak(p2, p2));
  EXPECT_EQ(1, RefCounted1::instances);
  EXPECT_TRUE(
      atomic.compare_exchange_strong(p2, MakeRefCounted<RefCounted1>()));
  EXPECT_EQ(2, RefCounted1::instances);
  EXPECT_EQ(12345, static_cast<RefPtr<RefCounted1>>(atomic)->xxx);
}

// Heap check should help us here to check if any leak occurs.
TEST(RefPtr, AtomicDontLeak) {
  RefPtr<RefCounted1> ps[2] = {nullptr, MakeRefCounted<RefCounted1>()};
  RefPtr<RefCounted1> temps[10];

  ASSERT_EQ(1, RefCounted1::instances);
  for (auto&& e : temps) {
    e = MakeRefCounted<RefCounted1>();
  }
  ASSERT_EQ(11, RefCounted1::instances);

  for (auto&& from : ps) {
    std::thread ts[10];
    std::atomic<RefPtr<RefCounted1>> atomic{from};
    std::atomic<bool> ever_success{false};

    ASSERT_EQ(11, RefCounted1::instances);
    for (auto&& e : ts) {
      e = std::thread([&] {
        while (!ever_success) {
          auto op = Random() % 4;
          if (op == 0) {
            atomic.store(temps[Random(9)], std::memory_order_release);
          } else if (op == 1) {
            if (auto ptr = atomic.load(std::memory_order_acquire)) {
              ASSERT_EQ(12345, ptr->xxx);
            }
          } else if (op == 2) {
            auto p1 = temps[0], p2 = temps[1], p3 = temps[2], p4 = temps[3];
            if (atomic.compare_exchange_strong(p1, temps[1],
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire)) {
              ever_success = true;
            }
            if (atomic.compare_exchange_weak(p2, temps[2],
                                             std::memory_order_acq_rel,
                                             std::memory_order_acquire)) {
              ever_success = true;
            }
            if (atomic.compare_exchange_strong(p3, temps[3],
                                               std::memory_order_acq_rel)) {
              ever_success = true;
            }
            if (atomic.compare_exchange_weak(p4, temps[4],
                                             std::memory_order_acq_rel)) {
              ever_success = true;
            }
          } else {
            atomic.exchange(temps[Random(9)]);
          }
          ASSERT_EQ(11, RefCounted1::instances);
        }
      });
    }
    for (auto&& e : ts) {
      e.join();
    }
    EXPECT_TRUE(ever_success.load());
  }
}

}  // namespace flare
