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

#include "flare/base/experimental/seqlocked.h"

#include <thread>

#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/base/chrono.h"
#include "flare/base/random.h"

using namespace std::literals;

namespace flare::experimental {

struct ABigPOD {
  int asc_seq[128];
};

Seqlocked<ABigPOD> a_big_pod;

std::atomic<bool> leaving_{};

ABigPOD NewValue() {
  ABigPOD result;
  auto start = Random(12345678);
  for (int i = 0; i != 128; ++i) {
    result.asc_seq[i] = i + start;
  }
  return result;
}

void Reader() {
  while (!leaving_.load(std::memory_order_relaxed)) {
    auto read = a_big_pod.Load();
    for (int i = 1; i != 128; ++i) {
      ASSERT_EQ(read.asc_seq[i], read.asc_seq[i - 1] + 1);
    }
  }
}

TEST(Seqlocked, All) {
  auto end = ReadCoarseSteadyClock() + 10s;
  a_big_pod.Store(NewValue());

  std::thread readers[32];
  for (auto&& t : readers) {
    t = std::thread(Reader);
  }

  while (ReadCoarseSteadyClock() < end) {
    a_big_pod.Store(NewValue());
    a_big_pod.Update([](ABigPOD* p) {
      for (auto&& e : p->asc_seq) {
        ++e;
      }
    });
  }
  leaving_.store(true, std::memory_order_relaxed);
  for (auto&& t : readers) {
    t.join();
  }
}

}  // namespace flare::experimental
