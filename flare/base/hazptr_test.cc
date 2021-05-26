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

#include "flare/base/hazptr.h"

#include <memory>
#include <thread>
#include <vector>

#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/base/chrono.h"
#include "flare/base/random.h"
#include "flare/base/string.h"
#include "flare/testing/main.h"

using namespace std::literals;

namespace flare {

struct Buffer : public HazptrObject<Buffer> {
  int x;
  ~Buffer() { x = 0; }
};

std::atomic<Buffer*> buffer_ptr;

inline void ReaderSide() {
  Hazptr ptr;
  auto p = ptr.Keep(&buffer_ptr);
  // Were the memory barrier removed from `Hazptr::TryKeep`, this assertion does
  // fire, albeit not quite often (a dozen of times in (each of) my runs.).
  ASSERT_EQ(1, p->x);
}

inline void WriterSide() {
  auto new_buffer = std::make_unique<Buffer>();
  new_buffer->x = 1;
  buffer_ptr.exchange(new_buffer.release(), std::memory_order_acq_rel)
      ->Retire();  // Install a new one and retire the old one.
}

TEST(Hazptr, All) {
  auto new_buffer = std::make_unique<Buffer>();
  new_buffer->x = 1;
  buffer_ptr = new_buffer.release();

  constexpr auto kTestDuration = 20s;
  std::vector<std::thread> readers, writers;
  auto start = ReadCoarseSteadyClock();

  for (int i = 0; i != 1; ++i) {
    readers.push_back(std::thread([&] {
      while (ReadCoarseSteadyClock() < start + kTestDuration) {
        ReaderSide();
      }
    }));
  }
  // You need a machine powerful enough (a multi-socket one might help further)
  // to run this UT. (I'm testing it on a 76C CVM.)
  for (int i = 0; i != 64; ++i) {
    writers.push_back(std::thread([&] {
      while (ReadCoarseSteadyClock() < start + kTestDuration) {
        WriterSide();
      }
    }));
  }
  for (auto&& e : readers) {
    e.join();
  }
  for (auto&& e : writers) {
    e.join();
  }
}

}  // namespace flare

FLARE_TEST_MAIN
