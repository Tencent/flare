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

#include "flare/base/thread/thread_local/ref_counted.h"

#include <thread>
#include <vector>

#include "thirdparty/googletest/gtest/gtest.h"

namespace flare::internal {

struct C : public RefCounted<C> {
  C() { ++instances; }
  ~C() { --instances; }
  inline static int instances = 0;
};

TEST(ThreadLocalRefCounted, All) {
  ThreadLocalRefCounted<C> tls;

  (void)*tls;
  EXPECT_EQ(1, C::instances);

  std::thread([&] {
    (void)*tls;
    EXPECT_EQ(2, C::instances);
  }).join();

  EXPECT_EQ(1, C::instances);

  std::atomic<bool> tls_initialized{};
  std::atomic<bool> ready_to_leave{};

  std::thread t2([&] {
    (void)*tls;
    EXPECT_EQ(2, C::instances);
    tls_initialized = true;
    while (!ready_to_leave) {
    }
  });

  while (!tls_initialized) {
  }

  EXPECT_EQ(2, C::instances);

  std::vector<RefPtr<C>> ptrs;
  tls.ForEach([&](C* ptr) { ptrs.push_back(RefPtr(ref_ptr, ptr)); });

  ready_to_leave = true;
  t2.join();

  EXPECT_EQ(2, C::instances);

  ptrs.clear();
  EXPECT_EQ(1, C::instances);
}

}  // namespace flare::internal
