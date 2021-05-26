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

#include "flare/base/object_pool/ref_counted.h"

#include <thread>

#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/testing/main.h"

using namespace std::literals;

namespace flare {

struct RefCounted1 : object_pool::RefCounted<RefCounted1> {
  RefCounted1() { ++instances; }
  ~RefCounted1() { --instances; }
  inline static int instances = 0;
};

template <>
struct PoolTraits<RefCounted1> {
  static constexpr auto kType = PoolType::ThreadLocal;
  static constexpr auto kLowWaterMark = 0;
  static constexpr auto kHighWaterMark = 128;
  static constexpr auto kMaxIdle = 100ms;
};

TEST(ObjectPoolRefCounted, All) {
  auto tid = std::this_thread::get_id();
  auto pp = object_pool::GetRefCounted<RefCounted1>();
  {
    auto p = object_pool::GetRefCounted<RefCounted1>();
    ASSERT_EQ(2, RefCounted1::instances);
    auto p2 = p;
    ASSERT_EQ(2, RefCounted1::instances);
  }
  {
    ASSERT_EQ(2, RefCounted1::instances);  // Not destroyed yet.
    auto p = object_pool::GetRefCounted<RefCounted1>();
    ASSERT_EQ(2, RefCounted1::instances);
    auto p2 = p;
    ASSERT_EQ(2, RefCounted1::instances);
  }
  // `this_fiber::SleepFor` WON'T work, object pools are bound to thread.
  std::this_thread::sleep_for(200ms);
  pp.Reset();  // To trigger cache washout.

  ASSERT_EQ(tid, std::this_thread::get_id());
  // The last one freed is kept alive by the pool.
  ASSERT_EQ(1, RefCounted1::instances);
}

}  // namespace flare

FLARE_TEST_MAIN
