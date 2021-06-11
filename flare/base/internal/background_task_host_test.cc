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

#include "flare/base/internal/background_task_host.h"

#include "googletest/gtest/gtest.h"

#include "flare/base/thread/latch.h"

namespace flare::internal {

TEST(BackgroundTaskHost, All) {
  int x = 1;
  Latch latch(1);
  BackgroundTaskHost::Instance()->Start();
  BackgroundTaskHost::Instance()->Queue([&] {
    x = 2;
    latch.count_down();
  });
  latch.wait();
  ASSERT_EQ(2, x);
  BackgroundTaskHost::Instance()->Stop();
  BackgroundTaskHost::Instance()->Join();
}

}  // namespace flare::internal
