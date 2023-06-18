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

// clang-format off
#include "flare/base/object_pool.h"
// clang-format on

#include <thread>
#include <vector>

#include "gtest/gtest.h"

using namespace std::literals;

namespace flare {

int alive = 0;

struct C {
  C() { ++alive; }
  ~C() { --alive; }
};

template <>
struct PoolTraits<C> {
  static constexpr auto kType = PoolType::ThreadLocal;
  static constexpr auto kLowWaterMark = 16;
  static constexpr auto kHighWaterMark = 128;
  static constexpr auto kMaxIdle = 3000ms;
};

struct D {
  int x;
  inline static int put_called = 0;
};

template <>
struct PoolTraits<D> {
  static constexpr auto kType = PoolType::ThreadLocal;
  static constexpr auto kLowWaterMark = 16;
  static constexpr auto kHighWaterMark = 128;
  static constexpr auto kMaxIdle = 3000ms;

  static void OnGet(D* p) { p->x = 0; }
  static void OnPut(D*) { ++D::put_called; }
};

namespace object_pool {

TEST(ThreadLocalPool, All) {
  std::vector<PooledPtr<C>> ptrs;
  for (int i = 0; i != 1000; ++i) {
    ptrs.push_back(Get<C>());
  }
  ptrs.clear();
  for (int i = 0; i != 200; ++i) {
    std::this_thread::sleep_for(10ms);
    Get<C>().Reset();  // Trigger wash out if possible.
  }
  ASSERT_EQ(PoolTraits<C>::kHighWaterMark, alive);  // High-water mark.

  // Max idle not reached. No effect.
  Get<C>().Reset();
  ASSERT_EQ(PoolTraits<C>::kHighWaterMark, alive);

  std::this_thread::sleep_for(5000ms);
  for (int i = 0; i != 100; ++i) {
    Get<C>().Reset();  // We have a limit on objects to loop per call. So we may
                       // need several calls to lower alive objects to low-water
                       // mark.
    std::this_thread::sleep_for(10ms);  // The limit on wash interval..
  }
  // Low-water mark. + 1 for the object just freed (as it's fresh and won't be
  // affected by low-water mark.).
  ASSERT_EQ(PoolTraits<C>::kLowWaterMark + 1, alive);

  Put<C>(Get<C>().Leak());  // Don't leak.
  ASSERT_EQ(PoolTraits<C>::kLowWaterMark + 1, alive);
}

TEST(ThreadLocalPool, OnGetHook) {
  { auto ptr = Get<D>(); }
  {
    auto ptr = Get<D>();
    ASSERT_EQ(0, ptr->x);
  }
  ASSERT_EQ(2, D::put_called);
}

}  // namespace object_pool
}  // namespace flare
